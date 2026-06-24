#!/bin/sh
# kbuild-cc.sh — incremental kernel compile for the standalone 64-bit kernel
# targets (kernel-aarch64 / kernel-x86_64), which use brute-force shell for-loops
# instead of bmake suffix rules and so have no native .d dependency tracking.
#
# Recompiles <obj> from <src> only when it is stale: the object is missing, the
# source is newer, or any prerequisite recorded in <obj>.d (a previous -MMD run)
# is newer or has been deleted.  An up-to-date object is left untouched and the
# step is silent, so an unchanged tree relinks without recompiling — a header
# edit recompiles just its dependents.
#
# Usage: kbuild-cc.sh <tag> <obj> <src> <cc> [cflags...]
#   <tag>  short label printed only when a compile actually runs (e.g. "[c]")
set -e

tag=$1
obj=$2
src=$3
shift 3
dep="${obj%.o}.d"

stale=0
if [ ! -f "$obj" ] || [ ! -f "$dep" ] || [ "$src" -nt "$obj" ]; then
	stale=1
else
	# Prerequisites: strip the "obj: " target off line 1 and trailing "\"
	# continuations; -MMD emits one rule (no -MP phony lines) so this is exact.
	for p in $(sed -e '1s/^[^:]*: *//' -e 's/[\\]$//' "$dep"); do
		if [ ! -e "$p" ] || [ "$p" -nt "$obj" ]; then
			stale=1
			break
		fi
	done
fi

[ "$stale" -eq 0 ] && exit 0

echo "$tag $src"
exec "$@" -MMD -MF "$dep" -c "$src" -o "$obj"
