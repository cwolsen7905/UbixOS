# (C) 2002-2026 The UbixOS Project
# ubix.platform.mk — Centralised host-platform detection.
#
# Included by Makefile.incl, lib/Makefile.incl, and bin/Makefile.incl.
# Safe to include from child makes: ?= and !defined guards prevent redundant
# shell invocations once variables are exported by the top-level make.
#
# Exports:
#   CROSS_PREFIX   cross-compiler prefix   (x86_64-elf- on Darwin/Linux x86_64, empty on FreeBSD i386)
#   CROSS_M32      -m32 flag               (set when the host is not natively i386)
#   LIBGCC         path to libgcc.a        (static libgcc for the link group)
#   GNU_MAKE       GNU make command        (musl's own Makefile requires GNU make, not bmake)

.if !defined(_UBIX_PLATFORM_MK)
_UBIX_PLATFORM_MK = 1

UNAME_S != uname -s
UNAME_M != uname -m

.if ${UNAME_S} == "Darwin"
# macOS: Homebrew provides x86_64-elf-gcc (ELF cross-compiler, not i686-elf-gcc).
# -m32 targets i386 ELF output from the 64-bit ELF toolchain.
CROSS_PREFIX ?= x86_64-elf-
CROSS_M32    ?= -m32
GNU_MAKE     ?= /usr/bin/make
.elif ${UNAME_S} == "Linux"
.  if ${UNAME_M} == "x86_64"
# Linux x86_64: native gcc produces ELF (no prefix needed), but -m32 is
# required to target i386.  Requires: gcc-multilib (Debian/Ubuntu) or
# gcc-multilib equivalent for your distro.
CROSS_PREFIX ?=
CROSS_M32    ?= -m32
GNU_MAKE     ?= make
.  else
# Linux i386 (native 32-bit host): no cross steps needed.
CROSS_PREFIX ?=
CROSS_M32    ?=
GNU_MAKE     ?= make
.  endif
.else
# FreeBSD and other BSDs: assumes native i386 host toolchain.
# On FreeBSD x86_64 you would need to set CROSS_M32=-m32 manually.
CROSS_PREFIX ?=
CROSS_M32    ?=
GNU_MAKE     ?= gmake
.endif

# Only compute LIBGCC once — child makes inherit it via the environment.
.if !defined(LIBGCC)
LIBGCC != ${CROSS_PREFIX}gcc ${CROSS_M32} -print-libgcc-file-name 2>/dev/null
.endif

# Per-arch musl/link knobs for the world build (mirrors ubix.musl.vars.mk, for
# the .incl files that don't pull vars.mk in).  _ARCH comes down from the
# top-level Makefile via WMAKE; default i386 so a bare invocation is unchanged.
#   MUSL_ARCH         musl per-arch header subdir under contrib/musl/arch/
#   MUSL_LDEMULATION  ld -m emulation
_ARCH ?= i386
.if ${_ARCH} == "aarch64"
MUSL_ARCH        ?= aarch64
MUSL_LDEMULATION ?= aarch64elf
.else
MUSL_ARCH        ?= i386
MUSL_LDEMULATION ?= elf_i386
.endif

.export CROSS_PREFIX CROSS_M32 LIBGCC GNU_MAKE MUSL_ARCH MUSL_LDEMULATION

.endif # _UBIX_PLATFORM_MK
