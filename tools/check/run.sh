#!/bin/sh
# run.sh — uBixOS gate driver.
#
#   tools/check/run.sh                  # fast tier on files changed vs base
#   tools/check/run.sh --all            # fast tier on the whole tree (baselined)
#   tools/check/run.sh --tier ci        # fast + slow checks (needs toolchains)
#   tools/check/run.sh format banned-api   # only the named checks
#   tools/check/run.sh --list
#   tools/check/run.sh --update-baseline banned-api
#
# Options:
#   --changed        files changed vs base (default)
#   --staged         files staged in the index (every line counts)
#   --file PATH      check exactly these files (repeatable; every line counts)
#   --all            every in-scope tracked file
#   --base REF       base ref for --changed (default: merge-base with origin/master|master)
#   --tier fast|ci   which set of checks to run when none are named (default fast)
#   --arch ARCH      restrict compile checks to one arch (repeatable)
#   --list           list checks and tiers, then exit
#   --update-baseline NAME   rewrite tools/check/baselines/NAME.txt from the current tree
#
# Exit: 0 all pass/skipped, 1 any check failed.  A check whose tool is missing
# is reported SKIP (and fails only with --strict, which CI uses).

set -u
CHECK_ROOT="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
export CHECK_ROOT
cd "$CHECK_ROOT" || exit 2
. "$CHECK_ROOT/tools/check/lib.sh"

CHECK_MODE=changed
CHECK_BASE=""
TIER=fast
STRICT=0
UPDATE_BASELINE=0
ARCHES=""
NAMED=""
EXPLICIT=""

# name:tier  (fast = cheap, every commit; ci = needs cross toolchains / long)
CHECK_TABLE="
format:fast
banned-api:fast
doxygen-docs:fast
file-scope-static:fast
syscall-table:fast
header-selfcontained:ci
warnings:ci
clang-tidy:ci
cppcheck:ci
build:ci
boot-smoke:ci
"

while [ $# -gt 0 ]; do
	case "$1" in
	--changed) CHECK_MODE=changed ;;
	--all) CHECK_MODE=all ;;
	--staged) CHECK_MODE=files; EXPLICIT="$(git diff --cached --name-only --diff-filter=ACMR)" ;;
	--file) shift; CHECK_MODE=files; EXPLICIT="$EXPLICIT
$1" ;;
	--base) shift; CHECK_BASE="$1" ;;
	--tier) shift; TIER="$1" ;;
	--arch) shift; ARCHES="$ARCHES $1" ;;
	--strict) STRICT=1 ;;
	--update-baseline) UPDATE_BASELINE=1 ;;
	--list) printf '%s' "$CHECK_TABLE" | awk -F: 'NF { printf "%-22s %s\n", $1, $2 }'; exit 0 ;;
	-h | --help) sed -n '2,22p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
	-*) echo "run.sh: unknown option $1" >&2; exit 2 ;;
	*) NAMED="$NAMED $1" ;;
	esac
	shift
done

if [ -n "$CHECK_BASE" ] && ! git rev-parse --verify -q "$CHECK_BASE^{commit}" >/dev/null; then
	CHECK_BASE=""	# e.g. the all-zero "before" SHA of a first push
fi
if [ -z "$CHECK_BASE" ]; then
	CHECK_BASE="$(git merge-base HEAD origin/master 2>/dev/null || git merge-base HEAD master 2>/dev/null || git rev-parse HEAD)"
fi
[ -z "$ARCHES" ] && ARCHES="aarch64 x86_64"
CHECK_ARCHES="$(echo $ARCHES)"

# Build the in-scope file list.
if [ "$CHECK_MODE" = all ]; then
	raw="$(git ls-files)"
elif [ "$CHECK_MODE" = files ]; then
	raw="$EXPLICIT"
else
	raw="$( { git diff --name-only --diff-filter=ACMR "$CHECK_BASE"; git ls-files --others --exclude-standard; } | sort -u)"
fi
CHECK_FILES=""
for f in $raw; do
	[ -f "$f" ] || continue
	check_is_c_source "$f" || continue
	check_in_scope "$f" || continue
	CHECK_FILES="$CHECK_FILES$f
"
done
export CHECK_MODE CHECK_BASE CHECK_FILES CHECK_ARCHES CHECK_UPDATE_BASELINE=$UPDATE_BASELINE

if [ -n "$NAMED" ]; then
	selected="$NAMED"
else
	selected="$(printf '%s' "$CHECK_TABLE" | awk -F: -v t="$TIER" 'NF && (t == "ci" || $2 == "fast") { print $1 }')"
fi

nfiles=$(printf '%s' "$CHECK_FILES" | grep -c . || true)
echo "check: mode=$CHECK_MODE base=$(echo "$CHECK_BASE" | cut -c1-10) files=$nfiles arches=$CHECK_ARCHES"

failed=0
for name in $selected; do
	script="$CHECK_ROOT/tools/check/checks/$name.sh"
	if [ ! -f "$script" ]; then
		echo "  ????  $name (no such check)"
		failed=1
		continue
	fi
	out="$(sh "$script" 2>&1)"
	rc=$?
	case $rc in
	0) printf '  ok    %s\n' "$name" ;;
	77)
		if [ "$STRICT" -eq 1 ]; then
			printf '  FAIL  %s (skipped: %s)\n' "$name" "$out"
			failed=1
		else
			printf '  skip  %s  %s\n' "$name" "$out"
		fi
		;;
	*)
		printf '  FAIL  %s\n' "$name"
		printf '%s\n' "$out" | sed 's/^/          /'
		failed=1
		;;
	esac
done

if [ $failed -eq 0 ]; then
	echo "check: all gates passed"
else
	echo "check: FAILED" >&2
fi
exit $failed
