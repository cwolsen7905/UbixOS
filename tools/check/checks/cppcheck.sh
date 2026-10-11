#!/bin/sh
# cppcheck — warning/portability findings on changed first-party sources;
# only findings on added lines fail.  Advisory in CI until baselines settle.
. "$CHECK_ROOT/tools/check/lib.sh"
cd "$CHECK_ROOT" || exit 2
command -v cppcheck >/dev/null 2>&1 || { echo "cppcheck not installed"; exit $CHECK_SKIP; }
tmp="$(mktemp)"; trap 'rm -f "$tmp" "$tmp.out"' EXIT
for f in $CHECK_FILES; do
	case "$f" in *.c | *.cc | *.cpp) ;; *) continue ;; esac
	added="$(added_lines "$f" | cut -d: -f1 | tr '\n' ' ')"
	[ -n "$added" ] || continue
	cppcheck --quiet --enable=warning,portability --inline-suppr --std=c11 \
		--template='{file}:{line}:{severity}:{id}:{message}' -Isys/include "$f" 2>"$tmp" >/dev/null
	grep -E "^$f:[0-9]+:" "$tmp" | while IFS=: read -r _ line rest; do
		case " $added " in *" $line "*) echo "$f:$line:$rest" ;; esac
	done
done >"$tmp.out"
[ -s "$tmp.out" ] && { cat "$tmp.out"; exit 1; }
exit 0
