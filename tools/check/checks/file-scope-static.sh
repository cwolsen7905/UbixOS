#!/bin/sh
# file-scope-static — file-scope data rules (CLAUDE.md "File-scope variable
# rules"): non-exported data is `static`, static data has the g_ prefix and
# sits at the top of the file before the first function.  Added lines only.
. "$CHECK_ROOT/tools/check/lib.sh"
cd "$CHECK_ROOT" || exit 2
[ "$CHECK_MODE" != all ] || { echo "n/a in --all mode"; exit 0; }
cand="$(mktemp)"; out="$(mktemp)"
trap 'rm -f "$cand" "$out"' EXIT
for f in $CHECK_FILES; do
	case "$f" in *.c | *.cc | *.cpp) ;; *) continue ;; esac
	added="$(added_lines "$f" | cut -d: -f1 | tr '\n' ' ')"
	[ -n "$added" ] || continue
	awk -f "$CHECK_ROOT/tools/check/funcscan.awk" "$f" | awk -F'\t' -v added="$added" -v f="$f" '
		BEGIN { n = split(added, a, " "); for (i = 1; i <= n; i++) A[a[i]] = 1 }
		$1 == "VAR" && ($2 in A) {
			name = $3; st = $4; cst = $5; after = $6
			if (!st) print f "\t" $2 "\t" name "\tnonstatic"
			else {
				if (!cst && name !~ /^g_/) print f "\t" $2 "\t" name "\tprefix"
				if (after) print f "\t" $2 "\t" name "\tlate"
			}
		}'
done >"$cand"
while IFS='	' read -r f line name kind; do
	case "$kind" in
	nonstatic)
		git grep -qw -e "$name" -- '*.h' 2>/dev/null && continue
		echo "$f:$line: '$name' is a non-static file-scope variable declared in no header — make it static" >>"$out" ;;
	prefix) echo "$f:$line: static variable '$name' must have the g_ prefix" >>"$out" ;;
	late) echo "$f:$line: static data '$name' is defined after the first function — move it to the top of the file" >>"$out" ;;
	esac
done <"$cand"
[ -s "$out" ] && { cat "$out"; exit 1; }
exit 0
