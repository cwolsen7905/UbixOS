# Lessons Carried Over from the i386 Port

i386 is frozen on `releng/2`. These notes were hard-won there and moved out of
`CLAUDE.md` so the index stays short. Details are specific to the 32-bit
kernel; the *principle* in each bullet still applies to the 64-bit ports
unless stated otherwise. See [kernel-rules.md](kernel-rules.md) for the
generalised rule.

- **No SIMD/FP in the kernel** (K-1). On i386 the kernel never set
  `CR4.OSFXSR`; GCC emitted XMM for struct copies and took `#UD`. Every kernel
  flag set carries `-mno-sse -mno-sse2 -mno-mmx -mno-3dnow` (x86_64) or
  `-mgeneral-regs-only` (aarch64).
- **MMIO frames are not RAM** (K-31). A physical address at or above
  `numPages * PAGE_SIZE` has no `vmmMemoryMap` entry. Fork's COW marking,
  `vmm_cleanVirtualSpace` and `freePage` must all check
  `(phys >> 12) >= numPages`; otherwise `freePage` indexes billions past the
  map and faults the kernel.
- **Kernel page-directory sync after fork.** `vmm_copyVirtualSpace` must copy
  the parent's kernel PDEs *after* its last `vmm_getFreeKernelPage`/
  `vmm_getFreePage`, since those can expand the parent's kernel directory and
  leave the child with stale zero entries. (i386 layout; the invariant — "snapshot
  shared kernel mappings after the last allocation that can change them" — is
  general.)
- **Load address vs the 640 KB hole.** The i386 kernel moved from `0x20000`
  to `0x300000` because BSS crossed `0xA0000` into the VGA/BIOS hole and
  lwIP's static pools landed in video memory. Fix the link address, do not
  shrink the pools to hide it.
- **lwIP sizing.** `MEM_SIZE` 32768; `PBUF_POOL_SIZE` 64 × 1514;
  `TCP_MSS` 1460, `TCP_WND`/`TCP_SND_BUF` 8×MSS; `MEMP_NUM_TCP_SEG` 64.
- **ABI width of callbacks.** A `vfsRead` function-pointer slot taking
  `off_t` must be implemented with `off_t` (a `long` misaligned the following
  argument on 32-bit and every procfs read returned 0). On LP64 this is a
  general rule: implementations match the declared prototype exactly (C-11).
- **`sprintf`/`kvprintf` terminator.** `kvprintf` never writes `'\0'`;
  `sprintf` must terminate explicitly.
- **Task naming.** `kTask_t` carries `name[256]`/`cmdline[1024]`; every
  task-creation path (`execFile`, `sys_exec`, `execThread`) must populate
  them.
- **Sockets.** `connect`/`bind`/`listen`/`accept` convert Linux-style
  `sockaddr` (no `sin_len`) to BSD-style for lwIP and **copy user data to kernel
  buffers first**: the lwIP `tcpip_thread` has no user mappings (K-40).
- **FAT library offsets.** The FAT library treats partition-relative sector 0
  as the BPB; `hdRead` adds `parOffset` — never add it twice.
- **TCC** needed `R_386_GOT32X` support (patched in `contrib/tcc`).
