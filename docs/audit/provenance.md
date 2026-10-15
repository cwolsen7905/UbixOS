# Provenance audit: FreeBSD / Berkeley / BSD-derived files in `sys/` and `lib/`

*Audited 2026-10-14 against `master` (e0d7182fa). Plan item A4 (IDs below are proposals; the desk assigns the real ones). Docs only: nothing in the tree was changed.*

The rule: kernel and native-library code is **clean room** (written from specs, no copied BSD or GPL source). Third-party code is allowed only under `contrib/` / `tools/ports/` with its licence intact ([third-party-ports-plan.md](../design/third-party-ports-plan.md)). **GPL code never enters the clean-room tree.**

## Flags (read first)

### GPL / unclear licence

- **No GPL, LGPL or AGPL text exists in `sys/` or `lib/`** (grep for `GNU General`, `GPL`, `Lesser`, `Affero`: zero hits). The clean-room trees are GPL-free today.
- **GPL outside the audited trees (needs the person's decision, not changed here):**
  - `contrib/busybox/` (58 tracked files): GPL-2.0. It is vendored and `Makefile` uses it to prove the relinked world runs. It is not linked into the kernel or native libraries, but a GPL userland binary shipped in the image should be a conscious, documented exception.
  - `usr.bin/doom/` (`doomgeneric_ubix.c`, `i_sound_ubix.c`, `i_music_ubix.c`, `opl2.c`, `opl2.h`): GPL-2.0, declared as such, "lives entirely inside the DOOM binary". Isolated from `lib/`, but it is GPL code in a first-party directory rather than `contrib/`.
  - `tools/ports/git`: git is GPLv2; a port recipe only, nothing vendored.
- **Unclear or non-standard licence terms in the audited set (all slated for deletion, so no relicensing work):**
  - `lib/libmd/md4*`, `md5*`, `*driver.c`, `*.copyright`: RSA Data Security notice. It permits use and derivative works only if identified as "derived from the RSA ... Message-Digest Algorithm" and the notice is kept. Not GPL, but a bespoke attribution licence.
  - `lib/libmd/{ripemd,rmd160c,sha,sha0c,sha1c}`: Eric Young (SSLeay) terms. The original SSLeay licence carries an advertising clause; the file text needs reading before any reuse.
  - `sys/include/sys/elf_common.h`: carries a UbixOS copyright but reproduces FreeBSD comment text. Provenance of the copyright claim is unclear; treated as `rewrite`.
  - `sys/fs/common/crc32.c` (not in the grep set, no BSD marker): "COPYRIGHT (C) 1986 Gary S. Brown ... as desired without restriction". Permissive but not an OSI licence; it is the same table FreeBSD's boot code carries. Goes away with `gpt.c`.
- **Scope gap, not audited here:** `libexec/rtld-elf/` (66 files, FreeBSD `rtld`) and `bin/`, `sbin/`, `usr.bin/` ports of FreeBSD utilities sit outside `sys/` and `lib/`. They need their own pass; `rtld-elf` is currently commented out of `libexec/Makefile`.
- **lwIP lives in `sys/`**: `sys/net/**` and `sys/include/{net,netif}/**` are lwIP (Adam Dunkels, BSD-3-Clause), not FreeBSD/Berkeley, so they are outside the 157-file set. They are third-party and by the rule belong in `contrib/` (the tree already has `contrib/lwip-2.0.3`). See Follow-ups.

## Method

Every file under `sys/` and `lib/` whose text matches `Regents of the University of California`, `FreeBSD Foundation`, `The FreeBSD Project`, `$FreeBSD`, `NetBSD`, `OpenBSD`, `Berkeley`, `FreeBSD`, `GNU General`, or `GPL` was collected (157 files). For each, usage was checked three ways: Makefile references (`lib/Makefile`, `libexec/Makefile`, `sys/fs/Makefile`, root `Makefile`, `tools/`), `#include` sites across `sys bin sbin usr.bin usr.sbin lib tests libexec`, and symbol callers. Two files matched only because they *mention* BSD names and are UbixOS-authored; they are listed under *False positives* and need no action.

## Summary

| Verdict | Files | Meaning |
|---|---:|---|
| keep | 3 | third-party, move to `contrib/` |
| rewrite | 3 | clean room, from specs |
| delete | 149 | unused: nothing builds or calls it |
| **total** | **155** | plus 2 false positives |

| Directory | keep | rewrite | delete |
|---|---:|---:|---:|
| `lib (top level)` |  |  | 1 |
| `lib/csu` |  |  | 49 |
| `lib/libedit` |  |  | 53 |
| `lib/libmd` |  |  | 31 |
| `sys/boot/arm` |  |  | 6 |
| `sys/fs/common` |  |  | 1 |
| `sys/include` |  | 1 |  |
| `sys/include/fs` |  |  | 5 |
| `sys/include/netif` | 3 |  |  |
| `sys/include/sys` |  | 2 | 2 |
| `sys/sys` |  |  | 1 |

Take-away: this is mostly dead weight, not live dependency. **Only 3 files are live BSD-derived code that must be rewritten**, all kernel headers. Everything else is unused and can be deleted without replacement.

## Proposed order

**Deletes first** (no replacement, shrink the audit to nothing but real work). Each is one PR; verify both arches build and boot after each, per P-1.

1. `lib/libmd/**` (31 files): unbuilt, resolves the RSA and SSLeay licence flags.
2. `lib/libedit/**` (53 files): unbuilt, NetBSD.
3. `lib/csu/**` + `lib/Makefile.inc` (50 files): also drop the `#csu-code` stub in `lib/Makefile`, and fix or remove `usr.bin/edit/Makefile` (unwired) which references `lib/csu/*.o`. Decide `libexec/rtld-elf` at the same time since it includes `lib/csu/common/notes.h`.
4. `sys/boot/arm/uboot/**` (6 flagged files; delete the whole directory, `conf.c` carries a Semihalf BSD notice with no FreeBSD marker), `sys/sys/syscall.mk`, `sys/include/fs/ufs/{ffs,ufs}/*.h`, `sys/include/sys/stdatomic.h`: unreferenced.
5. GPT group, deleted together: `gpt.c`, `gpt.h`, `crc32.h`, `uuid.h`, plus the now-orphan `crc32.c`, and the `common-code` entry in `sys/fs/Makefile` if the directory empties.

**Rewrites** (3 files), ordered by blast radius, smallest and most mechanical first:

1. `sys/include/assert.h`: trivial; ISO C `assert` mapped to the kernel panic path. Unblocks removing a Berkeley header from 21 includers with no source edits.
2. `sys/include/sys/cdefs.h`: write down which macros the 9 includers need (`__BEGIN_DECLS`, `__packed`, `__unused`, ...) and define only those. Independent of assert.h; do it second because it has a wider (but still small) includer set.
3. `sys/include/sys/elf_common.h`: largest (1340 lines). Regenerate from the ELF gABI and the aarch64/x86_64 psABIs; keep only the constants `elf32.h`, `elf64.h`, `elf64_load.c`, `elf64_demand.c` use. Do last: it needs the spec table pass and a boot test of exec on both arches.

**Conditional rewrites, not scheduled:** a GPT reader (UEFI spec) once real-hardware partitioning is needed; native `crt1` if musl's is ever replaced. Neither exists as a requirement today.

## Follow-ups (outside this lane's scope)

- Move lwIP out of `sys/` (`sys/net/**`, `sys/include/net`, `sys/include/netif`) into `contrib/lwip-*`; the 3 `keep` PPP headers go with it. Large; needs its own plan row.
- Audit `libexec/rtld-elf`, plus FreeBSD-derived utilities in `bin/ sbin/ usr.bin/ usr.sbin/`.
- Decide the busybox and doomgeneric GPL exceptions (person's call).
- Review `sys/fs/ufs/{ufs,ffs}.c` (no BSD marker, but they implement Berkeley FFS) and `sys/net/**` header comments for un-marked copied text.

## Proposed WORK.md row (for the desk; `docs/WORK.md` is not in this lane's scope)

Format per `docs/standards/work-matrix.md`:

```
| W-054 | provenance | Provenance audit: list FreeBSD/Berkeley-headed files in sys/ and lib/; keep/rewrite/delete | done | v3-provenance-audit | #PR | docs/audit/provenance.md |
| W-055 | provenance | Delete unused BSD-derived trees (libmd, libedit, csu, sys/boot, ...) | todo | - | - | audit order 1-5 |
| W-056 | provenance | Clean-room rewrite assert.h, cdefs.h, elf_common.h | todo | - | - | audit rewrite order |
```

## False positives (UbixOS-authored, no action)

- `sys/include/pci/hd.h`: partition-type name strings ("FreeBSD/NetBSD") only; UbixOS copyright
- `sys/kern/random.c`: cites OpenBSD arc4random in a comment only; UbixOS copyright

## File list

| File | Verdict | Reason |
|---|---|---|
| `lib/Makefile.inc` | delete | FreeBSD lib/Makefile.inc; not included by any Makefile (lib/Makefile.incl is ours). |
| `lib/csu/Makefile` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/Makefile.inc` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/aarch64/Makefile` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/aarch64/crt1.c` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/aarch64/crti.S` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/aarch64/crtn.S` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/amd64/Makefile` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/amd64/Makefile.depend` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/amd64/crt1.c` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/amd64/crti.S` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/amd64/crtn.S` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/arm/Makefile` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/arm/Makefile.depend` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/arm/crt1.c` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/arm/crti.S` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/arm/crtn.S` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/common/crtbrand.c` | delete | FreeBSD csu helper; only user is libexec/rtld-elf, which libexec/Makefile has commented out of the build. |
| `lib/csu/common/ignore_init.c` | delete | FreeBSD csu helper; only user is libexec/rtld-elf, which libexec/Makefile has commented out of the build. |
| `lib/csu/common/notes.h` | delete | FreeBSD csu helper; only user is libexec/rtld-elf, which libexec/Makefile has commented out of the build. |
| `lib/csu/i386/Makefile` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/i386/Makefile.depend` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/i386/crt1_c.c` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/i386/crt1_s.S` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/i386/crti.S` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/i386/crtn.S` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/mips/Makefile` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/mips/Makefile.depend` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/mips/crt1.c` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/mips/crti.S` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/mips/crtn.S` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/powerpc/Makefile` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/powerpc/Makefile.depend` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/powerpc/crt1.c` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/powerpc/crti.S` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/powerpc/crtn.S` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/powerpc64/Makefile` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/powerpc64/Makefile.depend` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/powerpc64/crt1.c` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/powerpc64/crti.S` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/powerpc64/crtn.S` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/riscv/Makefile` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/riscv/crt1.c` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/riscv/crti.S` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/riscv/crtn.S` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/sparc64/Makefile` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/sparc64/Makefile.depend` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/sparc64/crt1.c` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/sparc64/crti.S` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/csu/sparc64/crtn.S` | delete | FreeBSD csu (crt1/crti/crtn/Makefile); csu-code is commented out of lib/Makefile and musl supplies crt1 (lib/crt, lib/crtbegin are ours). |
| `lib/libedit/Makefile` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/Makefile.depend` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/TEST/rl1.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/TEST/tc1.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/TEST/wtc1.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/chared.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/chared.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/chartype.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/chartype.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/common.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/config.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/edit/readline/Makefile` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/edit/readline/Makefile.depend` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/edit/readline/history.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/edit/readline/readline.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/edit/readline/tilde.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/editline.3` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/editrc.5` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/el.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/el.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/eln.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/emacs.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/filecomplete.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/filecomplete.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/hist.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/hist.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/histedit.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/history.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/keymacro.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/keymacro.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/makelist` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/map.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/map.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/parse.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/parse.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/prompt.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/prompt.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/read.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/read.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/readline.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/refresh.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/refresh.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/search.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/search.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/sig.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/sig.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/sys.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/terminal.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/terminal.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/tokenizer.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/tty.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/tty.h` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libedit/vi.c` | delete | NetBSD libedit (BSD-2/3); not built by lib/Makefile and linked by nothing (tcsh/oksh bring their own editors). Re-import as a port if wanted. |
| `lib/libmd/Makefile` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). |
| `lib/libmd/Makefile.depend` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). |
| `lib/libmd/i386/rmd160.S` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). Eric Young SSLeay-style licence. |
| `lib/libmd/i386/sha.S` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). |
| `lib/libmd/md4.copyright` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). RSA Data Security notice-bearing code. |
| `lib/libmd/md4.h` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). RSA Data Security notice-bearing code. |
| `lib/libmd/md4c.c` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). RSA Data Security notice-bearing code. |
| `lib/libmd/md4hl.c` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). RSA Data Security notice-bearing code. |
| `lib/libmd/md5.copyright` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). RSA Data Security notice-bearing code. |
| `lib/libmd/md5.h` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). RSA Data Security notice-bearing code. |
| `lib/libmd/md5c.c` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). RSA Data Security notice-bearing code. |
| `lib/libmd/md5hl.c` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). RSA Data Security notice-bearing code. |
| `lib/libmd/mdX.3` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). |
| `lib/libmd/mdXhl.c` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). |
| `lib/libmd/mddriver.c` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). RSA Data Security notice-bearing code. |
| `lib/libmd/ripemd.3` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). Eric Young SSLeay-style licence. |
| `lib/libmd/ripemd.h` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). Eric Young SSLeay-style licence. |
| `lib/libmd/rmd160c.c` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). Eric Young SSLeay-style licence. |
| `lib/libmd/rmd160hl.c` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). Eric Young SSLeay-style licence. |
| `lib/libmd/rmddriver.c` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). RSA Data Security notice-bearing code. Eric Young SSLeay-style licence. |
| `lib/libmd/sha.3` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). |
| `lib/libmd/sha.h` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). Eric Young SSLeay-style licence. |
| `lib/libmd/sha0c.c` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). Eric Young SSLeay-style licence. |
| `lib/libmd/sha0hl.c` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). |
| `lib/libmd/sha1c.c` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). Eric Young SSLeay-style licence. |
| `lib/libmd/sha1hl.c` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). |
| `lib/libmd/sha256.3` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). |
| `lib/libmd/sha512.3` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). |
| `lib/libmd/shadriver.c` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). RSA Data Security notice-bearing code. |
| `lib/libmd/skein.3` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). |
| `lib/libmd/skeindriver.c` | delete | FreeBSD libmd; not built by lib/Makefile, no consumer (BearSSL + libpw provide hashes). RSA Data Security notice-bearing code. |
| `sys/boot/arm/uboot/Makefile` | delete | FreeBSD arm U-Boot loader; sys/boot is not in any build (tools/Makefile points at a nonexistent sys/boot/mbr). U-Boot is a port: tools/ports/u-boot. |
| `sys/boot/arm/uboot/help.uboot` | delete | FreeBSD arm U-Boot loader; sys/boot is not in any build (tools/Makefile points at a nonexistent sys/boot/mbr). U-Boot is a port: tools/ports/u-boot. |
| `sys/boot/arm/uboot/ldscript.arm` | delete | FreeBSD arm U-Boot loader; sys/boot is not in any build (tools/Makefile points at a nonexistent sys/boot/mbr). U-Boot is a port: tools/ports/u-boot. |
| `sys/boot/arm/uboot/loader.conf` | delete | FreeBSD arm U-Boot loader; sys/boot is not in any build (tools/Makefile points at a nonexistent sys/boot/mbr). U-Boot is a port: tools/ports/u-boot. |
| `sys/boot/arm/uboot/start.S` | delete | FreeBSD arm U-Boot loader; sys/boot is not in any build (tools/Makefile points at a nonexistent sys/boot/mbr). U-Boot is a port: tools/ports/u-boot. |
| `sys/boot/arm/uboot/version` | delete | FreeBSD arm U-Boot loader; sys/boot is not in any build (tools/Makefile points at a nonexistent sys/boot/mbr). U-Boot is a port: tools/ports/u-boot. |
| `sys/fs/common/gpt.c` | delete | FreeBSD boot-loader GPT code; linked into the kernel but no gpt_*() caller anywhere. Write a GPT reader from the UEFI spec when partitions are needed. |
| `sys/include/assert.h` | rewrite | Berkeley header; 21 kernel includers; a ~10-line clean-room macro from ISO C. |
| `sys/include/fs/common/crc32.h` | delete | FreeBSD prototype; only caller is the dead gpt.c (crc32.c itself is Gary S. Brown, unrestricted, and goes with it). |
| `sys/include/fs/common/gpt.h` | delete | FreeBSD boot-loader GPT code; linked into the kernel but no gpt_*() caller anywhere. Write a GPT reader from the UEFI spec when partitions are needed. |
| `sys/include/fs/ufs/ffs/fs.h` | delete | Berkeley FFS header; no includer (sys/fs/ufs uses its own ufs.h/ffs.h). |
| `sys/include/fs/ufs/ufs/ufs_extern.h` | delete | Berkeley FFS header; no includer (sys/fs/ufs uses its own ufs.h/ffs.h). |
| `sys/include/fs/ufs/ufs/ufsmount.h` | delete | Berkeley FFS header; no includer (sys/fs/ufs uses its own ufs.h/ffs.h). |
| `sys/include/netif/ppp/ipv6cp.h` | keep | lwIP PPP header (pppd/Berkeley lineage, not FreeBSD); PPP_SUPPORT=0; moves to contrib/ with lwIP (see Follow-ups). |
| `sys/include/netif/ppp/pppoe.h` | keep | lwIP PPP header (pppd/Berkeley lineage, not FreeBSD); PPP_SUPPORT=0; moves to contrib/ with lwIP (see Follow-ups). |
| `sys/include/netif/ppp/vj.h` | keep | lwIP PPP header (pppd/Berkeley lineage, not FreeBSD); PPP_SUPPORT=0; moves to contrib/ with lwIP (see Follow-ups). |
| `sys/include/sys/cdefs.h` | rewrite | Berkeley header; 9 kernel includers; reduce to the few __attribute__/__BEGIN_DECLS macros actually used. |
| `sys/include/sys/elf_common.h` | rewrite | UbixOS-claimed header but layout and comments (e.g. the DF_1_ORIGIN text) track FreeBSD verbatim; regenerate constants from the ELF gABI/psABI specs. |
| `sys/include/sys/stdatomic.h` | delete | FreeBSD (Schouten) C11 shim; nothing in sys/, bin/, lib/ includes it. |
| `sys/include/sys/uuid.h` | delete | FreeBSD (Moolenaar); only includer is the dead gpt.h. |
| `sys/sys/syscall.mk` | delete | FreeBSD libc syscall .mk; referenced by no Makefile. |
