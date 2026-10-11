#!/bin/sh
# header-selfcontained — each changed kernel header compiles on its own for
# every arch (coding.md C-16).  Userland headers are checked with the host
# compiler's syntax-only mode when include/ is touched.
. "$CHECK_ROOT/tools/check/lib.sh"
cd "$CHECK_ROOT" || exit 2
bad=0; ran=0
for f in $CHECK_FILES; do
	check_is_header "$f" || continue
	case "$f" in sys/include/*) ;; *) continue ;; esac
	for a in $CHECK_ARCHES; do
		spec="$(check_cc_for "$a")" || continue
		cc="${spec%%|*}"; fl="${spec#*|}"
		ran=1
		inc="${f#sys/include/}"
		# shellcheck disable=SC2086
		if ! printf '#include <%s>\n' "$inc" | $cc $fl -fsyntax-only -x c -Wno-error - >/tmp/hs.$$ 2>&1; then
			echo "$f is not self-contained on $a:"
			grep -m3 "error" /tmp/hs.$$ | sed 's/^/    /'
			bad=1
		fi
		rm -f /tmp/hs.$$
	done
done
[ $ran -eq 0 ] && [ $bad -eq 0 ] && [ -n "$(printf '%s' "$CHECK_FILES" | grep '^sys/include/')" ] && { echo "no cross compiler found"; exit $CHECK_SKIP; }
exit $bad
