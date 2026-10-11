#!/bin/sh
# banned-api — unsafe or layering-violating constructs (coding.md C-20,
# kernel-rules.md K-1/K-30/K-44, CLAUDE.md display rules).
#
# changed mode: any added line that matches fails.
# all mode:     per-file match counts are compared with
#               tools/check/baselines/banned-api.txt (may only go down).
. "$CHECK_ROOT/tools/check/lib.sh"
cd "$CHECK_ROOT" || exit 2
BASELINE="$CHECK_ROOT/tools/check/baselines/banned-api.txt"
tmp="$(mktemp)"; trap 'rm -f "$tmp"' EXIT

scan() { # FILE -> "FILE:LINE: [rule] text" for each hit on an added line
	f="$1"
	kern=0; check_is_kernel "$f" && kern=1
	case "$f" in
	usr.sbin/views/* | lib/libfb/* | tests/* | bin/init/* | usr.bin/doom/* | sys/*) fbok=1 ;;
	*) fbok=0 ;;
	esac
	case "$f" in
	sys/fs/* | sys/arch/*/kern/boot.c | sys/arch/*/boot/main.c | sys/arch/*/bringup/* | sys/include/*) fsok=1 ;;
	*) fsok=0 ;;
	esac
	added_lines "$f" | awk -v f="$f" -v kern="$kern" -v fbok="$fbok" -v fsok="$fsok" '
	{
		i = index($0, ":"); n = substr($0, 1, i - 1); t = substr($0, i + 1)
		c = t; sub(/^[ \t]+/, "", c)
		if (c ~ /^(\/\*|\*|\/\/)/) next
		sub(/\/\/.*$/, "", t); gsub(/"([^"\\]|\\.)*"/, "\"\"", t)
		if (t ~ /(^|[^A-Za-z0-9_.>])(strcpy|strcat|sprintf|vsprintf|gets|strtok)[ \t]*\(/)
			printf "%s:%s: [unsafe-string] %s\n", f, n, c
		if (kern && t ~ /(^|[^A-Za-z0-9_.>])(malloc|calloc|realloc|free|printf|fprintf|puts)[ \t]*\(/)
			printf "%s:%s: [hosted-libc-in-kernel] %s\n", f, n, c
		if (kern && t ~ /(^|[^A-Za-z0-9_])(float|double)[ \t]+[A-Za-z_*(]/)
			printf "%s:%s: [fp-in-kernel] %s\n", f, n, c
		if (!fbok && t ~ /(^|[^A-Za-z0-9_])sys_mapfb[ \t]*\(/)
			printf "%s:%s: [framebuffer-outside-views] %s\n", f, n, c
		if (!fsok && t ~ /^#[ \t]*include[ \t]*[<"]fs\/(fat|ufs|ubixfs|procfs|ramfs|devfs)\//)
			printf "%s:%s: [fs-driver-outside-vfs] %s\n", f, n, c
		if (t ~ /#[ \t]*pragma[ \t]+(GCC|clang)[ \t]+diagnostic[ \t]+ignored/)
			printf "%s:%s: [warning-suppression] %s\n", f, n, c
	}'
}

for f in $CHECK_FILES; do
	case "$f" in *.S) continue ;; esac
	scan "$f"
done >"$tmp"

if [ "$CHECK_MODE" = all ]; then
	counts="$(awk -F: '{ c[$1]++ } END { for (k in c) print k, c[k] }' "$tmp" | sort)"
	if [ "${CHECK_UPDATE_BASELINE:-0}" = 1 ]; then
		printf '%s\n' "$counts" >"$BASELINE"
		echo "baseline updated: $BASELINE ($(printf '%s\n' "$counts" | grep -c .) files)"
		exit 0
	fi
	bad=0
	printf '%s\n' "$counts" | while read -r file n; do
		[ -n "$file" ] || continue
		base="$(awk -v f="$file" '$1 == f { print $2 }' "$BASELINE" 2>/dev/null)"
		base="${base:-0}"
		if [ "$n" -gt "$base" ]; then
			echo "$file: $n banned-API hits, baseline allows $base"
			grep "^$file:" "$tmp"
			echo bad >"$tmp.bad"
		fi
	done
	[ -f "$tmp.bad" ] && { rm -f "$tmp.bad"; exit 1; }
	exit 0
fi

if [ -s "$tmp" ]; then
	cat "$tmp"
	echo "see docs/standards/coding.md (C-20), kernel-rules.md, and CLAUDE.md"
	exit 1
fi
exit 0
