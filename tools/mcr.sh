#!/bin/sh
# mcr.sh — Machine Code Review for UbixOS
#
# Runs clang-format (style check) and clang-tidy (static analysis) against
# C/C++ source files, then prints a summary.
#
# Usage:
#   tools/mcr.sh                    # files changed/new vs HEAD (default)
#   tools/mcr.sh --staged           # staged files only
#   tools/mcr.sh --all              # every tracked C/C++ file
#   tools/mcr.sh sys/vmm/paging.c  # specific files
#   tools/mcr.sh --fix [...]        # auto-apply clang-format + tidy fixes
#   tools/mcr.sh --format-only [...] # skip clang-tidy
#   tools/mcr.sh --tidy-only [...]  # skip clang-format

REPO_ROOT="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
cd "$REPO_ROOT"

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
CLANG_TIDY="${CLANG_TIDY:-/opt/homebrew/opt/llvm/bin/clang-tidy}"

# ── option parsing ────────────────────────────────────────────────────────────
FIX=0
RUN_FORMAT=1
RUN_TIDY=1
FILE_MODE="diff"
EXPLICIT_FILES=""

for arg in "$@"; do
  case "$arg" in
    --fix)           FIX=1 ;;
    --format-only)   RUN_TIDY=0 ;;
    --tidy-only)     RUN_FORMAT=0 ;;
    --staged)        FILE_MODE="staged" ;;
    --all)           FILE_MODE="all" ;;
    --help|-h)       sed -n '2,14p' "$0" | sed 's/^# \?//'; exit 0 ;;
    -*)              printf "Unknown option: %s  (try --help)\n" "$arg" >&2; exit 1 ;;
    *)               FILE_MODE="explicit"; EXPLICIT_FILES="$EXPLICIT_FILES $arg" ;;
  esac
done

# ── collect files ─────────────────────────────────────────────────────────────
case "$FILE_MODE" in
  diff)     FILES=$(git diff HEAD --name-only 2>/dev/null
                    git ls-files --others --exclude-standard 2>/dev/null) ;;
  staged)   FILES=$(git diff --cached --name-only) ;;
  all)      FILES=$(git ls-files) ;;
  explicit) FILES="$EXPLICIT_FILES" ;;
esac

# filter to C/C++ source only; skip contrib (third-party)
C_FILES=""
for f in $FILES; do
  case "$f" in
    contrib/*) continue ;;
    *.c|*.cpp|*.cc|*.h|*.hpp)
      [ -f "$f" ] && C_FILES="$C_FILES $f" ;;
  esac
done

if [ -z "$C_FILES" ]; then
  echo "mcr: no C/C++ files to check."
  exit 0
fi

file_count=$(echo $C_FILES | tr ' ' '\n' | grep -c .)
echo "mcr: checking $file_count file(s) [$FILE_MODE]"
echo ""

# ── include flags (kernel vs world) ──────────────────────────────────────────
kernel_flags="-m32 -nostdinc -Isys/include -D__KERNEL__"
world_flags="-m32 -Iinclude"

includes_for() {
  case "$1" in sys/*) echo "$kernel_flags" ;; *) echo "$world_flags" ;; esac
}

# ── clang-tidy checks ─────────────────────────────────────────────────────────
TIDY_CHECKS="\
readability-braces-around-statements,\
readability-redundant-semicolon,\
bugprone-macro-parentheses,\
bugprone-integer-overflow,\
clang-analyzer-core.NullDereference,\
clang-analyzer-core.UndefinedBinaryOperatorResult,\
clang-analyzer-unix.Malloc"

# ── run checks ────────────────────────────────────────────────────────────────
FORMAT_FAIL=0
FORMAT_FIXED=0
TIDY_FAIL=0
OVERALL=0

for f in $C_FILES; do
  file_ok=1

  # ── clang-format ───────────────────────────────────────────────────────────
  fmt_status=""
  if [ "$RUN_FORMAT" -eq 1 ]; then
    if [ "$FIX" -eq 1 ]; then
      "$CLANG_FORMAT" -i "$f"
      if git diff --quiet -- "$f" 2>/dev/null; then
        fmt_status="fmt:ok"
      else
        fmt_status="fmt:fixed"
        FORMAT_FIXED=$((FORMAT_FIXED + 1))
      fi
    else
      fmt_out=$("$CLANG_FORMAT" --dry-run --Werror "$f" 2>&1)
      fmt_exit=$?
      if [ "$fmt_exit" -eq 0 ]; then
        fmt_status="fmt:ok"
      else
        fmt_status="fmt:FAIL"
        FORMAT_FAIL=$((FORMAT_FAIL + 1))
        file_ok=0
      fi
    fi
  else
    fmt_status="fmt:skip"
  fi

  # ── clang-tidy (.c/.cpp/.cc only, not headers) ─────────────────────────────
  tidy_status=""
  tidy_out=""
  case "$f" in
    *.h|*.hpp)
      tidy_status="tidy:skip(hdr)"
      ;;
    *)
      if [ "$RUN_TIDY" -eq 1 ]; then
        inc=$(includes_for "$f")
        extra_args=""
        for flag in $inc; do
          extra_args="$extra_args --extra-arg=$flag"
        done
        fix_flag=""
        [ "$FIX" -eq 1 ] && fix_flag="--fix"
        tidy_out=$("$CLANG_TIDY" \
          $fix_flag \
          --checks="$TIDY_CHECKS" \
          --header-filter="^$REPO_ROOT/(sys|bin|lib|libexec|include)/" \
          --warnings-as-errors="*" \
          $extra_args \
          "$f" -- 2>&1)
        tidy_exit=$?
        if [ "$tidy_exit" -eq 0 ]; then
          tidy_status="tidy:ok"
        else
          tidy_status="tidy:FAIL"
          TIDY_FAIL=$((TIDY_FAIL + 1))
          file_ok=0
        fi
      else
        tidy_status="tidy:skip"
      fi
      ;;
  esac

  # ── per-file result ─────────────────────────────────────────────────────────
  if [ "$file_ok" -eq 1 ]; then
    printf "  ok    %-50s  %s  %s\n" "$f" "$fmt_status" "$tidy_status"
  else
    printf "  FAIL  %-50s  %s  %s\n" "$f" "$fmt_status" "$tidy_status"
    OVERALL=1
    if [ -n "$fmt_out" ]; then
      echo "$fmt_out" | sed 's/^/          /'
    fi
    if [ -n "$tidy_out" ]; then
      echo "$tidy_out" | grep -v "^[[:space:]]*$" | sed 's/^/          /'
    fi
  fi
done

# ── summary ───────────────────────────────────────────────────────────────────
echo ""
if [ "$FIX" -eq 1 ]; then
  echo "mcr: done  format-fixed=$FORMAT_FIXED"
  exit 0
elif [ "$OVERALL" -eq 0 ]; then
  echo "mcr: all checks passed"
  exit 0
else
  echo "mcr: FAILED  format=$FORMAT_FAIL  tidy=$TIDY_FAIL"
  echo "     run with --fix to auto-apply clang-format corrections"
  exit 1
fi
