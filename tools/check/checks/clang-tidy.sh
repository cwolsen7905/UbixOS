#!/bin/sh
# clang-tidy — tightened profile (.clang-tidy) on changed .c/.cc files, per-arch
# kernel flags, header filter limited to first-party code.  A finding fails only
# when it is on an added line.  Advisory in CI until baselines settle.
. "$CHECK_ROOT/tools/check/lib.sh"
cd "$CHECK_ROOT" || exit 2
TIDY="${CLANG_TIDY:-$(command -v clang-tidy || ls /opt/homebrew/opt/llvm/bin/clang-tidy 2>/dev/null)}"
[ -n "$TIDY" ] && [ -x "$TIDY" ] || { echo "clang-tidy not installed"; exit $CHECK_SKIP; }
tmp="$(mktemp)"; map="$(mktemp)"; trap 'rm -f "$tmp" "$tmp.out" "$map"' EXIT
a="${CHECK_ARCHES%% *}"
case "$a" in aarch64) tgt="--target=aarch64-none-elf -mgeneral-regs-only" ;; *) tgt="--target=x86_64-none-elf -mno-sse -mno-red-zone" ;; esac
# path:line for every added line of every changed file (headers included).
for f in $CHECK_FILES; do
	added_lines "$f" | cut -d: -f1 | sed "s|^|$f:|"
done >"$map"
for f in $CHECK_FILES; do
	case "$f" in sys/*.c | sys/*/*.c | sys/*/*/*.c | sys/*/*/*/*.c) ;; *) continue ;; esac
	# shellcheck disable=SC2086
	"$TIDY" --quiet "$f" -- $tgt -ffreestanding -nostdinc -Isys/include -Isys/arch/"$a" -D__KERNEL__ 2>/dev/null |
		sed "s|^$CHECK_ROOT/||" >"$tmp"
	grep -E '^[^ :]+:[0-9]+:[0-9]+: (warning|error):' "$tmp" | while IFS=: read -r path line _ rest; do
		grep -qxF "$path:$line" "$map" && echo "$path:$line:$rest"
	done
done | sort -u >"$tmp.out"
[ -s "$tmp.out" ] && { cat "$tmp.out"; exit 1; }
exit 0
