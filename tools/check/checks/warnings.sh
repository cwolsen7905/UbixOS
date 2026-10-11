#!/bin/sh
# warnings — -Wall -Wextra on every changed kernel .c for every arch; a warning
# fails only if it is located on a line the change added (ratchet: legacy
# warnings elsewhere in the same file or in headers do not block).
. "$CHECK_ROOT/tools/check/lib.sh"
cd "$CHECK_ROOT" || exit 2
bad=0; ran=0
tmp="$(mktemp)"; trap 'rm -f "$tmp"' EXIT
for f in $CHECK_FILES; do
	case "$f" in sys/*.c | sys/*/*.c | sys/*/*/*.c | sys/*/*/*/*.c) ;; *) continue ;; esac
	added="$(added_lines "$f" | cut -d: -f1 | tr '\n' ' ')"
	[ -n "$added" ] || continue
	for a in $CHECK_ARCHES; do
		case "$f" in sys/arch/aarch64/*) [ "$a" = aarch64 ] || continue ;; sys/arch/x86_64/*) [ "$a" = x86_64 ] || continue ;; esac
		spec="$(check_cc_for "$a")" || continue
		cc="${spec%%|*}"; fl="${spec#*|}"
		ran=1
		# shellcheck disable=SC2086
		$cc $fl -fsyntax-only -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare \
			-Wno-incompatible-pointer-types "$f" 2>"$tmp" >/dev/null
		grep -E "^$f:[0-9]+:[0-9]+: (warning|error):" "$tmp" | while IFS=: read -r _ line _ rest; do
			case " $added " in *" $line "*) echo "[$a] $f:$line:$rest" ;; esac
		done
	done
done >"$tmp.out"
[ -s "$tmp.out" ] && { cat "$tmp.out"; bad=1; }
rm -f "$tmp.out"
[ $ran -eq 0 ] && [ -n "$(printf '%s' "$CHECK_FILES" | grep '^sys/.*\.c$')" ] && { echo "no cross compiler found"; exit $CHECK_SKIP; }
exit $bad
