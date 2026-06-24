# (C) 2002-2026 The UbixOS Project
# ubix.toolchain.mk — single source of truth for the world toolchain.
#
# Included by Makefile.incl AFTER ubix.platform.mk + ubix.target.${_ARCH}.mk, so
# CROSS_PREFIX / MUSL_LDEMULATION are already resolved.  Selects between two
# toolchains via TOOLCHAIN (default gcc) and exports the tool names + the few
# compile/link flags that differ, so bin/lib/libexec Makefile.incl and the
# share/mk/ubix.musl.*.mk rules stay toolchain-agnostic:
#
#   gcc   — the cross GNU toolchain (aarch64-elf-/x86_64-elf-) used on the host.
#   clang — clang + ld.lld + llvm-* (the Stage-0 self-hosting toolchain).  Built
#           with LLVM_HOST_TRIPLE=${_ARCH}-unknown-linux-musl + CLANG_DEFAULT_LINKER=lld,
#           so in-OS the bare `clang` needs neither --target nor -fuse-ld.  The
#           tool names are single words so they pass cleanly through WORLD_FLAGS.
#
# The gcc branch reproduces the historical hardcoded values exactly, so a normal
# host build is byte-for-byte unchanged.  To VALIDATE the clang path on the host
# (where clang defaults to the macOS target), cross-drive a single subdir, e.g.:
#   bmake -C bin/cat TARGET=aarch64 TOOLCHAIN=clang \
#       CC="/opt/homebrew/opt/llvm@18/bin/clang --target=aarch64-unknown-linux-musl \
#           --ld-path=/opt/homebrew/opt/llvm@18/bin/ld.lld"
#
# Exports:
#   CC CXX AS AR LD NM OBJCOPY RANLIB   tool commands
#   TC_NOSTDINC                         -nostdinc (gcc) / -nostdlibinc (clang)
#   TC_STDFLAG                          empty (gcc16 defaults C23) / -std=gnu23 (clang18)

.if !defined(_UBIX_TOOLCHAIN_MK)
_UBIX_TOOLCHAIN_MK = 1

# Default toolchain.  Flipped to clang (2026-06-24) so a bare `bmake` builds the
# self-hosting toolchain by default — the way to shake out gcc→clang bugs in
# normal use.  Build with the legacy cross-GNU toolchain via `TOOLCHAIN=gcc`.
# NOTE: the kernel still *links* with binutils ld + gcc-built embeds under clang
# (only the compile is clang) until Phase E3 (ld.lld + llvm-objcopy) lands.
TOOLCHAIN ?= clang

.if ${TOOLCHAIN} == "clang"
# clang/lld/llvm-* profile.  In-OS the bare tools are in PATH and clang's default
# target + default linker (lld) are baked in.  On the host (Homebrew llvm@18) we
# cross-drive: point at that clang and inject --target + --ld-path so a host
# `bmake world/kernel TOOLCHAIN=clang` builds the aarch64/x86_64 system from macOS.
# CC carries the flags; that is safe because the world sub-makes recompute CC from
# the *exported* TOOLCHAIN (below), so a multi-word CC is never passed as a bare
# command-line word (which bmake would mis-split).  Hard '=' so these beat bmake's
# builtin CC=cc; a command-line CC= still overrides.
# Target triple + OS predefines.  The proven gcc world is built with
# aarch64-elf-gcc / x86_64-elf-gcc, whose only OS-ish predefine is __ELF__.  We
# need TWO things and they pull in opposite directions:
#   - a triple clang lowers through its GNU/ELF link driver (a *-none-elf triple
#     makes clang pick the *Darwin* linker for x86_64 on a macOS host → it feeds
#     ld.lld Mach-O flags like -arch/-platform_version/-dynamic).  -linux-musl
#     uses the GNU driver and lld accepts its flags, on both arches.
#   - gcc's exact predefines: -linux-musl also defines __linux__/__unix(__)/
#     __gnu_linux__, which flip ports (tcsh, …) onto Linux-only code paths the
#     bare-metal gcc never compiled.  So strip them with _UNDEF.
# Net: -linux-musl for the linker driver, _UNDEF to match gcc's macro set.  Both
# are inert for the in-OS clang (its default IS linux-musl), so host == on-device.
_LLVM18 = /opt/homebrew/opt/llvm@18/bin
_TGT     = --target=${_ARCH}-unknown-linux-musl
_UNDEF   = -U__linux__ -U__unix__ -U__unix -U__gnu_linux__
.if exists(${_LLVM18}/clang)
_CLANG   ?= ${_LLVM18}/clang
_CLANGXX ?= ${_LLVM18}/clang++
_LLVMBIN ?= ${_LLVM18}/
_LDPATH  ?= --ld-path=${_LLVM18}/ld.lld
.else
_CLANG   ?= clang
_CLANGXX ?= clang++
_LLVMBIN ?=
_LDPATH  ?=
.endif
CC       = ${_CLANG} ${_TGT} ${_UNDEF} ${_LDPATH}
CXX      = ${_CLANGXX} ${_TGT} ${_UNDEF} ${_LDPATH}
AS       = ${_CLANG} ${_TGT} ${_UNDEF}
AR       = ${_LLVMBIN}llvm-ar
LD       = ${_LLVMBIN}ld.lld
NM       = ${_LLVMBIN}llvm-nm
OBJCOPY  = ${_LLVMBIN}llvm-objcopy
RANLIB   = ${_LLVMBIN}llvm-ranlib
# Kernel compiler split into binary + flags: the kernel recipe passes the
# compiler as a single argument to kbuild-cc.sh, so the multi-word world CC
# (clang --target … --ld-path …) can't be used there.  KERN_CC is the bare clang
# binary; KERN_CCFLAGS carries --target + the OS-macro -U's (no --ld-path: the
# kernel links with ${LD} directly, not via the clang driver).
KERN_CC      = ${_CLANG}
KERN_CCFLAGS = ${_TGT} ${_UNDEF}
# clang's -nostdinc would also drop its resource headers (stdbool.h, stddef.h);
# -nostdlibinc keeps them while still excluding the host's system headers.
TC_NOSTDINC ?= -nostdlibinc
# busybox/world C uses bare bool/true/false (C23); GCC 16 defaults to C23 but
# clang 18 defaults to gnu17 — pin gnu23 to match.
TC_STDFLAG  ?= -std=gnu23
# lld's aarch64 little-endian emulation name (GNU ld's is aarch64elf).
.if ${_ARCH} == "aarch64"
MUSL_LDEMULATION = aarch64linux
.endif
.else
# Default: the cross GNU toolchain selected by platform.mk + target.${_ARCH}.mk.
# Hard '=' so these win over bmake's builtin CC=cc; command-line still overrides.
CC       = ${CROSS_PREFIX}gcc
CXX      = ${CROSS_PREFIX}g++
AS       = ${CROSS_PREFIX}as
AR       = ${CROSS_PREFIX}ar
LD       = ${CROSS_PREFIX}ld
NM       = ${CROSS_PREFIX}nm
OBJCOPY  = ${CROSS_PREFIX}objcopy
RANLIB   = ${CROSS_PREFIX}ranlib
KERN_CC      = ${CROSS_PREFIX}gcc
KERN_CCFLAGS =
TC_NOSTDINC ?= -nostdinc
TC_STDFLAG  ?=
.endif

# Export TOOLCHAIN so world sub-makes (which re-include this file) recompute the
# same toolchain instead of inheriting a possibly multi-word CC as a command word.
.export TOOLCHAIN CC CXX AS AR LD NM OBJCOPY RANLIB TC_NOSTDINC TC_STDFLAG KERN_CC KERN_CCFLAGS

.endif # _UBIX_TOOLCHAIN_MK
