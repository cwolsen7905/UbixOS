#!/bin/sh
# buildlock.sh — cross-instance build coordination for UbixOS.
#
# Two agents/sessions share this repo.  A full `bmake world`/`image` recompiles
# the whole tree (and re-configures shared contrib like busybox/musl), so two
# running at once race and corrupt each other.  This serialises them with an
# atomic lock (mkdir is atomic across processes), so there is no message bus
# needed — both sides just take the lock before building and drop it after.
#
# Usage:
#   tools/buildlock.sh acquire "<who/what>"   # 0 if taken, 1 if already held
#   tools/buildlock.sh release
#   tools/buildlock.sh status
#   tools/buildlock.sh steal                  # force-clear a stale lock
#   tools/buildlock.sh with "<who>" <cmd...>  # acquire, run cmd, always release
#
# Typical:
#   tools/buildlock.sh with "aural-aarch64" bmake world image

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
LOCK="$ROOT/.buildlock"
OWNER="$LOCK/owner"

stamp() { date '+%Y-%m-%d %H:%M:%S'; }

case "$1" in
acquire)
	if mkdir "$LOCK" 2>/dev/null; then
		printf 'who=%s pid=%s since=%s\n' "${2:-unknown}" "$$" "$(stamp)" >"$OWNER"
		echo "build-lock: ACQUIRED by ${2:-unknown}"
		exit 0
	fi
	echo "build-lock: BUSY — held by: $(cat "$OWNER" 2>/dev/null || echo '?')"
	exit 1
	;;
release)
	rm -rf "$LOCK" && echo "build-lock: released"
	;;
status)
	if [ -d "$LOCK" ]; then
		echo "build-lock: HELD — $(cat "$OWNER" 2>/dev/null || echo '?')"
	else
		echo "build-lock: free"
	fi
	;;
steal)
	rm -rf "$LOCK" && echo "build-lock: stale lock cleared"
	;;
with)
	who="$2"
	shift 2
	if ! mkdir "$LOCK" 2>/dev/null; then
		echo "build-lock: BUSY — held by: $(cat "$OWNER" 2>/dev/null || echo '?'); not running '$*'"
		exit 1
	fi
	printf 'who=%s pid=%s since=%s\n' "${who:-unknown}" "$$" "$(stamp)" >"$OWNER"
	echo "build-lock: ACQUIRED by ${who:-unknown} — running: $*"
	# Always release, even on failure/interrupt.
	trap 'rm -rf "$LOCK"' EXIT INT TERM
	"$@"
	rc=$?
	rm -rf "$LOCK"
	trap - EXIT INT TERM
	echo "build-lock: released (exit $rc)"
	exit $rc
	;;
*)
	echo "usage: $0 {acquire <who>|release|status|steal|with <who> <cmd...>}" >&2
	exit 2
	;;
esac
