#!/bin/sh
# format — clang-format conformance of every touched source file.
# Whole-file, per CLAUDE.md ("format every file you touch").  Fix with
# `tools/mcr.sh --fix` or `clang-format -i <file>`.
. "$CHECK_ROOT/tools/check/lib.sh"
cd "$CHECK_ROOT" || exit 2
CF="${CLANG_FORMAT:-clang-format}"
command -v "$CF" >/dev/null 2>&1 || { echo "clang-format not installed"; exit $CHECK_SKIP; }
bad=0
for f in $CHECK_FILES; do
	case "$f" in *.S) continue ;; esac
	if ! "$CF" --dry-run --Werror "$f" >/dev/null 2>&1; then
		echo "not clang-format clean: $f"
		bad=1
	fi
done
exit $bad
