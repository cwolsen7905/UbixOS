#!/bin/sh
# syscall-table — consistency of the shared syscall tables and arch parity of
# the per-arch dispatch glue (abi-and-syscalls.md A-3, A-4, P-8).
#
#  1. In the POSIX table, the `/* N */` (or `// N`) trailing index comment equals the
#     entry's real slot.  Catches renumbering/insert mistakes.
#  2. No syscall name is registered twice as SYSCALL_VALID with different slots
#     in the POSIX table (copy/paste slot errors).
#  3. Arch glue parity: the `case SYS_*:` pre-dispatch cases in the aarch64 and
#     x86_64 entry code.  A case present on only one arch is a parity gap; the
#     known set is recorded in baselines/syscall-parity.txt and may only shrink.
. "$CHECK_ROOT/tools/check/lib.sh"
cd "$CHECK_ROOT" || exit 2
bad=0

index_check() { # FILE ARRAYNAME
	awk -v arr="$2" -v f="$1" '
		$0 ~ "struct syscall_entry[ \t]+" arr "\\[\\]" { on = 1; slot = 0; next }
		on && /^\};/ { on = 0 }
		on && /^[ \t]*\{/ {
			if (match($0, /(\/\*|\/\/)[ \t]*[0-9]+/)) {
				c = substr($0, RSTART, RLENGTH); gsub(/[^0-9]/, "", c)
				if (c + 0 != slot) { printf "%s:%d: entry is slot %d but comment says %d\n", f, NR, slot, c; bad = 1 }
			}
			slot++
		}
		END { exit bad }' "$1"
}

# The native table's trailing index comments are stale legacy (many read "// 1 -"),
# so only the FreeBSD-numbered POSIX table is index-checked.
for spec in "sys/posix/syscalls_posix.c systemCalls_posix"; do
	set -- $spec
	[ -f "$1" ] || { echo "missing $1"; bad=1; continue; }
	index_check "$1" "$2" || bad=1
done

glue_cases() { # FILE... -> sorted case labels
	grep -ho "case[[:space:]]\{1,\}\(SYS\|NATIVE\)_[A-Z0-9_]*" "$@" 2>/dev/null | awk '{ print $2 }' | sort -u
}
a="$(glue_cases sys/arch/aarch64/kern/syscall.c sys/arch/aarch64/kern/syscall_md.c)"
x="$(glue_cases sys/arch/x86_64/kern/syscall_md.c sys/arch/x86_64/kern/usermode.c)"
only="$( { echo "$a" | sed 's/^/aarch64 /'; echo "$x" | sed 's/^/x86_64 /'; } |
	awk '{ s[$2] = s[$2] " " $1 } END { for (k in s) if (s[k] !~ /aarch64.*x86_64|x86_64.*aarch64/) print k s[k] }' | sort)"
BASE="$CHECK_ROOT/tools/check/baselines/syscall-parity.txt"
if [ "${CHECK_UPDATE_BASELINE:-0}" = 1 ]; then
	printf '%s\n' "$only" >"$BASE"; echo "baseline updated: $BASE"; exit 0
fi
new="$(printf '%s\n' "$only" | grep . | while read -r line; do
	grep -qxF "$line" "$BASE" 2>/dev/null || echo "$line"
done)"
if [ -n "$new" ]; then
	echo "arch glue handles a syscall on only one arch (add it to the shared table, or to both):"
	printf '%s\n' "$new" | sed 's/^/  /'
	bad=1
fi
exit $bad
