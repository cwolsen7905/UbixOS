#!/bin/sh
# mcr.sh — Machine Code Review for uBixOS.
#
# Thin front end over tools/check/run.sh (the real gate suite): runs the format
# gate, and (unless --format-only) the Doxygen, file-scope, banned-API and
# clang-tidy gates on the selected files.  Kept for muscle memory; CI calls
# tools/check/run.sh directly.
#
# Usage:
#   tools/mcr.sh                      # files changed vs the merge base (default)
#   tools/mcr.sh --staged             # staged files only
#   tools/mcr.sh --all                # every tracked source file (baselined gates)
#   tools/mcr.sh path/to/file.c ...   # specific files
#   tools/mcr.sh --fix [...]          # clang-format -i the selected files, then check
#   tools/mcr.sh --format-only [...]  # only the clang-format gate
#   tools/mcr.sh --tidy-only [...]    # only clang-tidy
#
# Environment: CLANG_FORMAT, CLANG_TIDY override tool discovery.

REPO_ROOT="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
cd "$REPO_ROOT" || exit 2

FIX=0
GATES="format banned-api doxygen-docs file-scope-static clang-tidy"
SELECT=""
FILES=""

for arg in "$@"; do
	case "$arg" in
	--fix) FIX=1 ;;
	--format-only) GATES="format" ;;
	--tidy-only) GATES="clang-tidy" ;;
	--staged) SELECT="--staged" ;;
	--all) SELECT="--all" ;;
	--help | -h) sed -n '2,19p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
	-*) echo "mcr: unknown option $arg (try --help)" >&2; exit 1 ;;
	*) FILES="$FILES --file $arg" ;;
	esac
done
[ -n "$FILES" ] && SELECT="$FILES"

if [ "$FIX" -eq 1 ]; then
	CF="${CLANG_FORMAT:-clang-format}"
	if [ -n "$FILES" ]; then
		list="$(echo "$FILES" | sed 's/--file //g')"
	elif [ "$SELECT" = "--staged" ]; then
		list="$(git diff --cached --name-only --diff-filter=ACMR)"
	else
		list="$( { git diff --name-only --diff-filter=ACMR HEAD; git ls-files --others --exclude-standard; } | sort -u)"
	fi
	for f in $list; do
		case "$f" in contrib/* ) continue ;; *.c | *.cc | *.cpp | *.h | *.hpp) [ -f "$f" ] && "$CF" -i "$f" ;; esac
	done
	echo "mcr: clang-format applied"
fi

# shellcheck disable=SC2086
exec tools/check/run.sh $SELECT $GATES
