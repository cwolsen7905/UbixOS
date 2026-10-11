#!/bin/sh
# build — kernel + world for every arch (arch-parity.md P-1).  Always through
# tools/buildlock.sh when run locally so concurrent sessions don't race on the
# shared tree.  In CI (CI=true) there is a single runner, so no lock.
. "$CHECK_ROOT/tools/check/lib.sh"
cd "$CHECK_ROOT" || exit 2
command -v bmake >/dev/null 2>&1 || { echo "bmake not installed"; exit $CHECK_SKIP; }
WHAT="${CHECK_BUILD_TARGETS:-kernel world}"
for a in $CHECK_ARCHES; do
	echo "== bmake $WHAT TARGET=$a"
	# shellcheck disable=SC2086
	if [ "${CI:-}" = true ]; then
		bmake $WHAT TARGET="$a" || { echo "build failed: $a"; exit 1; }
	else
		tools/buildlock.sh with "check-build-$a" bmake $WHAT TARGET="$a" || { echo "build failed: $a"; exit 1; }
	fi
done
exit 0
