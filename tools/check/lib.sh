#!/bin/sh
# lib.sh — shared helpers for the tools/check gate suite.  Sourced, not run.
#
# Environment provided by run.sh to every check:
#   CHECK_ROOT   repo root
#   CHECK_MODE   "changed" (default), "all", or "files" (explicit list; every line counts)
#   CHECK_BASE   git ref the changed set is computed against
#   CHECK_FILES  newline-separated, in-scope, existing source files
#   CHECK_ARCHES arches to compile for (default "aarch64 x86_64")
#
# Check exit codes: 0 pass, 1 fail, 77 skipped (required tool missing).

CHECK_SKIP=77

# First-party source trees the gates apply to.  Vendored code (contrib/, FreeBSD
# imports under lib/ and libexec/) is never checked; fix it upstream.
check_in_scope() {
	case "$1" in
	contrib/* | build/* | lib/libedit/* | lib/libmd/* | lib/zlib/* | lib/csu/* | lib/libbearssl/* | \
		lib/libcss/* | lib/libdom/* | lib/libhubbub/* | lib/libnsbmp/* | lib/libnsfb/* | lib/libnsgif/* | \
		lib/libnsutils/* | lib/libparserutils/* | lib/libutf8proc/* | lib/libwapcaplet/* | \
		libexec/rtld-elf/* | sys/arch/armv6/*) return 1 ;;
	sys/* | bin/* | sbin/* | usr.bin/* | usr.sbin/* | lib/* | libexec/* | tests/* | include/* | tools/*) return 0 ;;
	esac
	return 1
}

check_is_c_source() {
	case "$1" in *.c | *.cc | *.cpp | *.h | *.hpp | *.S) return 0 ;; esac
	return 1
}

check_is_header() {
	case "$1" in *.h | *.hpp) return 0 ;; esac
	return 1
}

# True if the file is under the kernel tree (sys/), where stricter rules apply.
check_is_kernel() {
	case "$1" in sys/*) return 0 ;; esac
	return 1
}

# Per-arch compiler + flags for syntax-only kernel/header compiles.  Echoes
# "<cc>|<flags>" or nothing when the cross compiler is not installed.
check_cc_for() {
	_arch="$1"
	case "$_arch" in
	aarch64)
		_cc="${AARCH64_CC:-aarch64-elf-gcc}"
		_fl="-march=armv8-a -mgeneral-regs-only"
		;;
	x86_64)
		_cc="${X86_64_CC:-x86_64-elf-gcc}"
		_fl="-mno-sse -mno-sse2 -mno-mmx -mno-3dnow -mno-red-zone -mcmodel=large"
		;;
	*) return 1 ;;
	esac
	command -v "$_cc" >/dev/null 2>&1 || return 1
	echo "$_cc|$_fl -ffreestanding -fno-builtin -fno-pic -nostdinc -fno-strict-aliasing -I$CHECK_ROOT/sys/include -I$CHECK_ROOT/sys/arch/$_arch"
}

# added_lines FILE — print "<lineno>:<text>" for each line the change adds.
# In "all" mode, or for an untracked/new file, every line is "added".
added_lines() {
	_f="$1"
	if [ "$CHECK_MODE" != changed ] || ! git -C "$CHECK_ROOT" ls-files --error-unmatch -- "$_f" >/dev/null 2>&1; then
		awk '{ print NR ":" $0 }' "$CHECK_ROOT/$_f"
		return
	fi
	git -C "$CHECK_ROOT" diff -U0 --no-color "$CHECK_BASE" -- "$_f" | awk '
		/^@@/ { s = $3; sub(/^\+/, "", s); split(s, p, ","); n = p[1] + 0; next }
		/^\+\+\+/ { next }
		/^\+/ { print n ":" substr($0, 2); n++ }'
}

# fail MSG... — record a finding (prints to stderr, bumps the counter file).
check_fail() {
	printf '%s\n' "$*" >&2
	CHECK_FINDINGS=$((CHECK_FINDINGS + 1))
}

CHECK_FINDINGS=0
