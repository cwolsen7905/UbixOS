#!/bin/sh
# selftest.sh — proves each gate fails on known-bad input and passes on
# known-good input, in a scratch git repo.  Run after editing any check.
#
#   tools/check/selftest.sh
set -u
SRC="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
cd "$T" || exit 2
git init -q . && git config user.email t@t && git config user.name t
mkdir -p tools sys/foo sys/posix sys/arch/aarch64/kern sys/arch/x86_64/kern sys/kern
cp -R "$SRC/tools/check" tools/check
cp "$SRC/.clang-format" .clang-format
mkdir -p tools/check/baselines
for d in sys/foo sys/posix sys/arch/aarch64/kern sys/arch/x86_64/kern sys/kern; do : >"$d/.keep"; done
git add -A && git commit -qm base
pass=0; fail=0

# expect STATUS CHECK DESCRIPTION  (runs only CHECK against the working tree)
expect() {
	want="$1"; chk="$2"; desc="$3"
	out="$(tools/check/run.sh --base HEAD "$chk" 2>&1)"; rc=$?
	if [ "$want" = fail ] && [ $rc -ne 0 ]; then ok=1
	elif [ "$want" = pass ] && [ $rc -eq 0 ]; then ok=1
	else ok=0; fi
	if [ $ok -eq 1 ]; then pass=$((pass + 1)); echo "  ok    $desc"
	else fail=$((fail + 1)); echo "  FAIL  $desc (wanted $want, rc=$rc)"; echo "$out" | sed 's/^/        /'; fi
	git -C "$T" clean -fdq -- sys >/dev/null
}

cat >sys/foo/bad.c <<'X'
void f(char *d, const char *s)
{
	strcpy(d, s);
}
X
expect fail banned-api "banned-api rejects strcpy"
cat >sys/foo/good.c <<'X'
/**
 * Copy bounded.
 */
void f(char *d, const char *s, unsigned long n)
{
	strlcpy(d, s, n);
}
X
expect pass banned-api "banned-api accepts strlcpy"

cat >sys/foo/nodoc.c <<'X'
int add(int a, int b)
{
	return a + b;
}
X
expect fail doxygen-docs "doxygen-docs rejects undocumented function"
cat >sys/foo/doc.c <<'X'
/**
 * Add two ints.
 *
 * @return the sum.
 */
int add(int a, int b)
{
	return a + b;
}
X
expect pass doxygen-docs "doxygen-docs accepts documented function"

cat >sys/foo/glob.c <<'X'
int counter;
X
expect fail file-scope-static "file-scope-static rejects non-static global"
cat >sys/foo/late.c <<'X'
static int g_a;

void f(void)
{
}

static int g_late;
X
expect fail file-scope-static "file-scope-static rejects static data after a function"
cat >sys/foo/pfx.c <<'X'
static int counter;
X
expect fail file-scope-static "file-scope-static rejects static without g_"
cat >sys/foo/ok.c <<'X'
static int g_counter;
static const char *names[] = {"a"};

/**
 * Do nothing.
 */
void f(void)
{
}
X
expect pass file-scope-static "file-scope-static accepts well-formed file"

if command -v clang-format >/dev/null 2>&1; then
	printf 'int   main( void ){return 0;}\n' >sys/foo/ugly.c
	expect fail format "format rejects badly formatted file"
	printf 'int   main( void ){return 0;}\n' >sys/foo/pretty.c
	clang-format -i sys/foo/pretty.c
	expect pass format "format accepts clean file"
else
	echo "  skip  format (clang-format missing)"
fi

cat >sys/posix/syscalls_posix.c <<'X'
struct syscall_entry systemCalls_posix[] = {
    {0, "No Call", sys_invalid, SYSCALL_VALID}, /* 0 */
    {0, "No Call", sys_invalid, SYSCALL_VALID}, /* 5 */
};
X
: >sys/arch/aarch64/kern/syscall.c; : >sys/arch/x86_64/kern/usermode.c
expect fail syscall-table "syscall-table rejects wrong slot index comment"
cat >sys/posix/syscalls_posix.c <<'X'
struct syscall_entry systemCalls_posix[] = {
    {0, "No Call", sys_invalid, SYSCALL_VALID}, /* 0 */
    {0, "No Call", sys_invalid, SYSCALL_VALID}, /* 1 */
};
X
expect pass syscall-table "syscall-table accepts consistent table"

echo "selftest: $pass passed, $fail failed"
[ $fail -eq 0 ]
