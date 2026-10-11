#!/bin/sh
# doxygen-docs — every function definition added or whose signature changed in
# the diff must be preceded by a /** ... */ block (CLAUDE.md, coding.md C-40).
# Only checked in "changed" mode; whole-tree enforcement would be noise.
. "$CHECK_ROOT/tools/check/lib.sh"
cd "$CHECK_ROOT" || exit 2
[ "$CHECK_MODE" != all ] || { echo "n/a in --all mode"; exit 0; }
bad=0
for f in $CHECK_FILES; do
	case "$f" in *.S) continue ;; esac
	added="$(added_lines "$f" | cut -d: -f1 | tr '\n' ' ')"
	[ -n "$added" ] || continue
	awk -f "$CHECK_ROOT/tools/check/funcscan.awk" "$f" | awk -F'\t' -v added="$added" -v f="$f" '
		BEGIN { n = split(added, a, " "); for (i = 1; i <= n; i++) A[a[i]] = 1 }
		$1 == "FUNC" {
			touched = 0
			for (l = $2; l <= $3; l++) if (l in A) touched = 1
			if (touched && $5 == 0) { printf "%s:%s: function %s() has no /** */ doc block\n", f, $2, $4; bad = 1 }
		}
		END { exit bad }' || bad=1
done
[ $bad -eq 0 ] || echo "format: /** brief ... @return ... */ — see CLAUDE.md 'Mandatory rules for all new code'"
exit $bad
