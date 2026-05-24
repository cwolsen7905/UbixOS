# FAT Driver Audit Findings

## Summary

17 findings: 2 critical, 5 high, 6 medium, 4 low

---

## Findings

### FAT-001 — FAT16 sector-boundary straddle in `fat_cluster_next` (read path)
**Severity:** 🔴 Critical
**File:** `sys/fs/fat/fat_clust.c:123`
**Description:** The FAT16 read case uses `le16(buf + offset_in_sector)` without checking whether `offset_in_sector == 511`. If it is, `le16` reads `buf[511]` and `buf[512]`, one byte past the end of the 512-byte stack buffer. The FAT12 path handles this case explicitly with a two-sector read; FAT16 does not.
**Impact:** Out-of-bounds stack read. The high byte of the FAT entry comes from whatever happens to follow `buf` on the stack, yielding a garbage cluster number. This causes chain corruption, loops, or skipped data — reliably triggered on any partition where a FAT16 entry straddles a sector boundary.
**Suggested fix:** Add the same straddle guard used in the FAT12 path before the `le16` call in case 16.

---

### FAT-002 — FAT16 sector-boundary straddle in `fat_cluster_write_entry` (write path)
**Severity:** 🔴 Critical
**File:** `sys/fs/fat/fat_clust.c:203`
**Description:** The FAT16 write case calls `put_le16(buf + offset_in_sector, ...)`. If `offset_in_sector == 511`, `put_le16` writes `buf[511]` and `buf[512]`, overflowing the 512-byte stack buffer by one byte.
**Impact:** Stack buffer overflow. Overwrites the return address or a local variable in the caller's frame, causing a kernel panic or silent control-flow corruption.
**Suggested fix:** Mirror the FAT12 two-sector read-modify-write logic for the FAT16 case when `offset_in_sector == 511`.

---

### FAT-003 — `fat_cluster_alloc` wrap-around loop misses the last valid cluster
**Severity:** 🟠 High
**File:** `sys/fs/fat/fat_clust.c:267`
**Description:** The wrap-around loop runs `for (c = 2; c < start; c++)`. If `start == total_clusters + 1` (the hint was pointing at the last cluster), the primary loop terminates because `c == total_clusters + 2 > total_clusters + 1`, and the wrap loop runs `c < total_clusters + 1`, which excludes cluster `total_clusters + 1` — the last valid cluster is never scanned.
**Impact:** Spurious "disk full" when the only free cluster is the last one on the volume.
**Suggested fix:** Change the wrap loop upper bound to `c <= fs->total_clusters + 1`.

---

### FAT-004 — `fat_cluster_free_chain` does not stop at `FAT_CLUSTER_BAD`
**Severity:** 🟠 High
**File:** `sys/fs/fat/fat_clust.c:301`
**Description:** The free-chain loop terminates on `c < 2` or `c == FAT_CLUSTER_EOC` but not on `FAT_CLUSTER_BAD` (`0x0FFFFFF7`). If a chain contains a bad-cluster marker, the loop passes that value to `fat_cluster_write_entry`, which computes a FAT sector at LBA `0x0FFFFFF7 * 4 / 512 ≈ 33 million` — a sector that doesn't exist, causing an I/O error cascade or (on a permissive driver) FAT corruption.
**Impact:** I/O error cascade or disk corruption when freeing any file whose cluster chain touches a bad-cluster marker.
**Suggested fix:** Add `c != FAT_CLUSTER_BAD` to the loop condition, or more robustly, treat any value outside the range `[2, total_clusters + 1]` as a chain terminator.

---

### FAT-005 — `fat_cluster_to_lba` wraps on cluster < 2
**Severity:** 🟠 High
**File:** `sys/fs/fat/fat_clust.c:74`
**Description:** `data_lba + (cluster - 2) * sectors_per_cluster`. For `cluster = 0` or `1`, the unsigned subtraction wraps to `0xFFFFFFFE`/`0xFFFFFFFF`, yielding an enormous LBA that can be silently passed to the block driver.
**Impact:** Any path that calls `fat_cluster_to_lba` with cluster 0 or 1 causes a runaway disk read/write, potential kernel crash. The directory `iter_sector` for the FAT32 root could pass cluster 2 or higher through legitimate code, but corrupted FAT entries can cause chain-traversal to return 0 or 1, which are then used directly.
**Suggested fix:** Add a guard at the top of `fat_cluster_to_lba` returning a sentinel (e.g. 0) and logging an error for `cluster < 2`.

---

### FAT-006 — Integer overflow in `fat_cluster_to_lba` for large FAT32 volumes
**Severity:** 🟠 High
**File:** `sys/fs/fat/fat_clust.c:76`
**Description:** `(cluster - 2) * fs->sectors_per_cluster` is computed in `uint32_t`. For `sectors_per_cluster = 128` and `cluster > 33,554,434`, the product overflows 32 bits and wraps to a small LBA, silently directing I/O to the wrong location.
**Impact:** Silent data corruption or wrong-data reads on large FAT32 volumes with large cluster sizes.
**Suggested fix:** Cast `(cluster - 2)` to `uint64_t` before multiplying, or validate during BPB parse that `total_clusters * sectors_per_cluster` fits in 32 bits.

---

### FAT-007 — LFN buffer NUL-termination missing for sequences near the 255-char limit
**Severity:** 🟠 High
**File:** `sys/fs/fat/fat_dir.c:366`
**Description:** The inner write guard `pos + i < 255` skips writing `it->lfn[255]` (a valid array index since `lfn[256]`). The NUL guard `pos + 13 < 256` means no NUL is written when `pos >= 243` (seq_num ≥ 19). `it->lfn` is then unterminated beyond index 254. A subsequent `strncpy(name_out, it->lfn, 255)` still NUL-terminates `name_out`, but the iterator field itself is not safe for direct comparison or reuse.
**Impact:** Garbled or unterminated filenames for names longer than ~242 characters in the iterator's internal buffer. `fat_names_equal` could read past the intended string boundary.
**Suggested fix:** Use `pos + i < 256` (array bound) as the write guard and always set `it->lfn[255] = '\0'` unconditionally.

---

### FAT-008 — `fat_dir_create_entry` LFN iterator reconstruction can miscompute `sector_in_cluster`
**Severity:** 🟠 High
**File:** `sys/fs/fat/fat_dir.c:543`
**Description:** The LFN write loop manually computes `it.sector_in_cluster = first_sec - fat_cluster_to_lba(fs, dir_cluster)`. This assumes `first_sec` belongs to the first cluster of `dir_cluster`'s chain. If the free run starts in a later cluster of a multi-cluster directory, `fat_cluster_to_lba(dir_cluster)` is the LBA of the first cluster, giving a `sector_in_cluster` value exceeding `sectors_per_cluster`. When `iter_advance` is called, the check `sector_in_cluster >= sectors_per_cluster` immediately follows the FAT chain from `dir_cluster` rather than from the cluster that actually contains `first_sec`, landing on the wrong cluster.
**Impact:** Directory entries written to the wrong cluster in directories spanning more than one cluster, producing a corrupted directory structure.
**Suggested fix:** Walk the iterator forward from the start of the directory to `first_sec:first_off` rather than manually reconstructing the iterator position.

---

### FAT-009 — `fat_dir_delete_entry`/`fat_dir_unlink` orphan LFN entries
**Severity:** 🟡 Medium
**File:** `sys/fs/fat/fat_dir.c:449`
**Description:** Only the SFN slot is marked `0xE5`. The preceding LFN entries retain their original first-byte values (sequence numbers), so `find_free_slots` counts them as used entries. The iterator correctly ignores them when scanning (because it clears LFN state on `0xE5`), but `find_free_slots` cannot reuse them for new entries.
**Impact:** Directory fills with unreclaimable LFN slots after repeated create/delete cycles; `fat_dir_create_entry` fails even though logically there is space.
**Suggested fix:** In `fat_dir_unlink` and `fat_dir_rmdir`, walk backward from the SFN entry and mark all matching LFN entries (same checksum) with `0xE5` before marking the SFN.

---

### FAT-010 — `fat_path_resolve` silently splits over-long path components
**Severity:** 🟡 Medium
**File:** `sys/fs/fat/fat_dir.c:648`
**Description:** Component extraction stops at 255 characters (`i < 255`) but does not advance `p` past the remaining characters. The unconsumed characters are treated as a new path component by the next loop iteration, causing a spurious second lookup in the same directory rather than returning a clean error.
**Impact:** A user-supplied path with a component > 255 characters produces an incorrect (and confusing) ENOENT rather than ENAMETOOLONG. This path arrives from kernel VFS with a userspace-derived string.
**Suggested fix:** After the extraction loop, if `i == 255 && *p && *p != '/'`, consume the rest of the component and return `-1`.

---

### FAT-011 — FAT32 directories cannot grow when full (`find_free_slots` does not extend)
**Severity:** 🟡 Medium
**File:** `sys/fs/fat/fat_dir.c:499`
**Description:** `find_free_slots` returns `-1` if no contiguous run of `n_slots` free entries is found. For FAT32, a directory is a cluster chain and can be extended by allocating a new cluster. The function never attempts this extension.
**Impact:** File creation fails with a spurious "no free slots" error in any FAT32 directory that has been used and freed entries are fragmented, even when free disk space exists.
**Suggested fix:** When `find_free_slots` fails on a FAT32 directory (cluster ≠ 0), allocate a new cluster, zero it, append it to the directory chain, and retry the search.

---

### FAT-012 — BPB does not validate `bytes_per_sector` must equal 512
**Severity:** 🟡 Medium
**File:** `sys/fs/fat/fat_bpb.c:99`
**Description:** The driver hard-codes 512-byte sector buffers everywhere but does not reject BPBs reporting `bytes_per_sector` other than 512. A disk with 4096-byte logical sectors would mount without error while every sector address calculation produces wrong results.
**Impact:** Silent data corruption or misread on disks with non-512-byte logical sectors.
**Suggested fix:** Replace `bytes_per_sec == 0` check with `bytes_per_sec != 512`.

---

### FAT-013 — BPB does not validate `fat_sz > 0`
**Severity:** 🟡 Medium
**File:** `sys/fs/fat/fat_bpb.c:104`
**Description:** If both `fat_sz16` and `fat_sz32` are zero, `fat_sz == 0`. `fs->root_lba` equals `rsvd_sec`, overlapping with the reserved region, and all FAT sector lookups read from the wrong region.
**Impact:** Silently mounts a malformed image; all FAT chain reads/writes operate on the wrong sectors.
**Suggested fix:** Add `|| fat_sz == 0` to the existing validation guard.

---

### FAT-014 — BPB does not validate `tot_sec > data_lba`
**Severity:** 🟡 Medium
**File:** `sys/fs/fat/fat_bpb.c:105`
**Description:** If `tot_sec == 0` (both fields zero), `data_sec = 0 - data_lba` wraps to a huge unsigned value, making `total_clusters` enormous. The cluster allocator would loop for billions of iterations.
**Impact:** Kernel hangs during mount of a malformed image.
**Suggested fix:** Validate `tot_sec > fs->data_lba` after computing `fs->data_lba`; return `-1` if not.

---

### FAT-015 — Global single-instance `_fatfs` prevents mounting more than one FAT volume
**Severity:** 🔵 Low
**File:** `sys/fs/fat/fat_vfs.c:45`
**Description:** `_fatfs` is a single static struct. A second call to `fat_initialize` overwrites the first partition's geometry. Currently only one FAT volume (`sys:/`) is mounted.
**Impact:** Latent: mounting a second FAT partition silently corrupts both.
**Suggested fix:** `kmalloc` a `struct fat_fs` per call to `fat_initialize`, store the pointer in `mp->fs_private`, and dereference it in each VFS callback.

---

### FAT-016 — `write_fat` ignores return value of `fat_file_write`, reports success on failure
**Severity:** 🔵 Low
**File:** `sys/fs/fat/fat_vfs.c:157`
**Description:** `fat_file_write`'s return value is discarded. If the write fails (disk full, I/O error), `write_fat` still returns `(int)size`, signaling success to the VFS.
**Impact:** Silent write loss; callers believe all bytes were written.
**Suggested fix:** Check `fat_file_write`'s return value and propagate `-1` on failure.

---

### FAT-017 — Dead code: `dot_sec`/`dot_off` set but never used in `fat_dir_mkdir`
**Severity:** 🔵 Low
**File:** `sys/fs/fat/fat_dir.c:803`
**Description:** Variables `dot_sec` and `dot_off` are assigned but immediately suppressed with `(void)dot_sec; (void)dot_off`. The redundant `memset(buf, 0, 32)` before the `.` entry is also a no-op because `buf` was just read from a freshly zeroed sector.
**Impact:** Code quality only; no functional impact.
**Suggested fix:** Remove the two dead variables, the `(void)` casts, and the redundant `memset`.
