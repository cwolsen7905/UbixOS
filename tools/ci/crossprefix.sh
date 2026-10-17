#!/bin/sh
# crossprefix.sh ARCH — print the distro cross-compiler prefix for ARCH.
#
# Linux has no *-elf- triplets, so CI passes the distro compilers to bmake as
# CROSS_PREFIX (the target makefiles assign it; the command line overrides).
# x86_64 uses x86_64-linux-gnu- because tools/build-netsurf.sh defaults an
# empty prefix to x86_64-elf-.
case "$1" in
aarch64) echo aarch64-linux-gnu- ;;
x86_64) echo x86_64-linux-gnu- ;;
*) echo "usage: $0 aarch64|x86_64" >&2; exit 2 ;;
esac
