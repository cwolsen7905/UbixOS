#!/bin/sh
# tests/pkgdev/run.sh — black-box tests of the on-device pkg client (W-071).
#
# usage: run.sh <pkg> <mkpkg> <workdir>
#
# Builds test packages with mkpkg, runs pkg against a scratch --root and checks
# add / del / info / verify, and that bad signatures, tampering, conflicts and
# failed installs leave nothing behind.  Exit status 0 means every check passed.

PKG=$1
MKPKG=$2
W=$3
FAILS=0
CHECKS=0

[ -x "$PKG" ] && [ -x "$MKPKG" ] && [ -n "$W" ] || { echo "usage: run.sh pkg mkpkg workdir"; exit 2; }
rm -rf "$W"
mkdir -p "$W"

ok() { CHECKS=$((CHECKS + 1)); echo "ok   $1"; }
bad() { CHECKS=$((CHECKS + 1)); FAILS=$((FAILS + 1)); echo "FAIL $1"; }
# expect <description> <expected-exit-0|1> command...
expect() {
	_d=$1; _want=$2; shift 2
	"$@" >"$W/out" 2>&1
	_got=$?
	[ "$_want" = 0 ] && [ $_got -eq 0 ] && { ok "$_d"; return; }
	[ "$_want" = 1 ] && [ $_got -ne 0 ] && { ok "$_d"; return; }
	bad "$_d (exit $_got)"; sed 's/^/     | /' "$W/out"
}
has() { grep -q -- "$2" "$W/out" && ok "$1" || { bad "$1 (missing '$2')"; sed 's/^/     | /' "$W/out"; }; }
# tree <root> lists every path below root except the var/ bookkeeping dirs
tree() { (cd "$1" && find . -mindepth 1 -not -path './var*' | sort); }

ARCH=$(uname -m); [ "$ARCH" = arm64 ] && ARCH=aarch64
ARCH=$("$PKG" --arch 2>/dev/null || echo "$ARCH")

# --- fixtures ---------------------------------------------------------------
"$MKPKG" keygen "$W/good" && "$MKPKG" keygen "$W/evil" || { echo "keygen failed"; exit 1; }
mkdir -p "$W/s1/usr/local/bin" "$W/s1/usr/local/share/hello" "$W/s1/usr/local/etc"
printf '#!/bin/sh\necho hello from pkg\n' >"$W/s1/usr/local/bin/hello"; chmod 755 "$W/s1/usr/local/bin/hello"
printf 'greeting\n' >"$W/s1/usr/local/share/hello/data.txt"
ln -s hello "$W/s1/usr/local/bin/hi"
mkdir -p "$W/s1/usr/local/share/hello/a/very/deep/directory/tree/that/goes/on/and/on/until/the/ustar/name/limit/is/exceeded"
printf 'deep\n' >"$W/s1/usr/local/share/hello/a/very/deep/directory/tree/that/goes/on/and/on/until/the/ustar/name/limit/is/exceeded/file.txt"

"$MKPKG" build "$W/hello.upkg" hello 1.0 "$ARCH" "$W/s1" && "$MKPKG" sign "$W/good.key" "$W/hello.upkg" || exit 1

newroot() { R="$W/root.$1"; rm -rf "$R"; mkdir -p "$R/etc/pkg/keys" "$R/usr/local"; cp "$W/good.pub" "$R/etc/pkg/keys/"; }

# --- add / info / verify / del ----------------------------------------------
newroot 1
expect "add --local installs a signed package" 0 "$PKG" --root "$R" add --local "$W/hello.upkg"
[ -x "$R/usr/local/bin/hello" ] && ok "payload file present and executable" || bad "payload file present"
[ -L "$R/usr/local/bin/hi" ] && ok "symlink present" || bad "symlink present"
[ -f "$R/usr/local/share/hello/a/very/deep/directory/tree/that/goes/on/and/on/until/the/ustar/name/limit/is/exceeded/file.txt" ] \
	&& ok "long (pax) path extracted" || bad "long (pax) path extracted"
[ -f "$R/var/db/pkg/hello/MANIFEST" ] && [ -f "$R/var/db/pkg/hello/FILES" ] && ok "database record written" || bad "database record written"
[ ! -e "$R/var/lock/pkg.lock" ] && [ ! -e "$R/var/db/pkg/.journal" ] && ok "lock and journal released" || bad "lock and journal released"
find "$R" -name '*.upkg-tmp' | grep -q . && bad "no temp files left" || ok "no temp files left"

expect "info lists the package" 0 "$PKG" --root "$R" info
has "info list shows name-version" "hello-1.0-r1"
expect "info <name> shows manifest and files" 0 "$PKG" --root "$R" info hello
has "info <name> shows the comment" "test package hello"
has "info <name> lists files" "usr/local/bin/hello"
expect "info of an unknown package fails" 1 "$PKG" --root "$R" info nothere

expect "verify passes on a clean install" 0 "$PKG" --root "$R" verify
expect "verify <name> passes" 0 "$PKG" --root "$R" verify hello
expect "add of an installed package is refused" 1 "$PKG" --root "$R" add --local "$W/hello.upkg"

echo tampered >>"$R/usr/local/share/hello/data.txt"
expect "verify fails on a tampered file" 1 "$PKG" --root "$R" verify hello
has "verify names the tampered file" "MODIFIED usr/local/share/hello/data.txt"
rm "$R/usr/local/bin/hi"
expect "verify fails on a missing file" 1 "$PKG" --root "$R" verify
has "verify reports the missing link" "MISSING usr/local/bin/hi"

expect "del removes the package" 0 "$PKG" --root "$R" del hello
has "del keeps the modified file" "keeping modified usr/local/share/hello/data.txt"
[ ! -e "$R/usr/local/bin/hello" ] && ok "unmodified files removed" || bad "unmodified files removed"
[ -f "$R/usr/local/share/hello/data.txt" ] && ok "modified file kept" || bad "modified file kept"
[ ! -e "$R/var/db/pkg/hello" ] && ok "record dropped" || bad "record dropped"
expect "del of a removed package fails" 1 "$PKG" --root "$R" del hello

newroot 2
"$PKG" --root "$R" add --local "$W/hello.upkg" >/dev/null 2>&1
expect "del --force removes modified files" 0 "$PKG" --root "$R" --force del hello
[ -z "$(find "$R/usr/local" -type f -o -type l)" ] && ok "force leaves no files" || bad "force leaves no files"

# --- signatures -------------------------------------------------------------
newroot 3
cp "$W/hello.upkg" "$W/unsigned.upkg"
expect "add refuses a package with no signature" 1 "$PKG" --root "$R" add --local "$W/unsigned.upkg"
cp "$W/hello.upkg" "$W/evil.upkg"; "$MKPKG" sign "$W/evil.key" "$W/evil.upkg"
expect "add refuses a signature from an untrusted key" 1 "$PKG" --root "$R" add --local "$W/evil.upkg"
cp "$W/hello.upkg" "$W/mod.upkg"; cp "$W/hello.upkg.sig" "$W/mod.upkg.sig"; printf 'x' >>"$W/mod.upkg"
expect "add refuses a package changed after signing" 1 "$PKG" --root "$R" add --local "$W/mod.upkg"
[ ! -d "$R/var/db/pkg/hello" ] && [ -z "$(find "$R/usr/local" -type f)" ] && ok "refused adds install nothing" || bad "refused adds install nothing"
expect "add --no-verify installs an unsigned package" 0 "$PKG" --root "$R" --no-verify add --local "$W/unsigned.upkg"
has "--no-verify warns" "without verifying"
rm -rf "$R"; newroot 3; rm "$R/etc/pkg/keys/good.pub"
expect "add refuses when there is no trust list" 1 "$PKG" --root "$R" add --local "$W/hello.upkg"

# --- conflicts --------------------------------------------------------------
newroot 4
mkdir -p "$R/usr/local/bin"; echo mine >"$R/usr/local/bin/hello"
expect "add refuses to overwrite an unowned file" 1 "$PKG" --root "$R" add --local "$W/hello.upkg"
has "conflict names the path" "usr/local/bin/hello"
[ "$(cat "$R/usr/local/bin/hello")" = mine ] && ok "unowned file untouched" || bad "unowned file untouched"
[ ! -d "$R/var/db/pkg/hello" ] && [ ! -e "$R/usr/local/share" ] && ok "refused add leaves nothing" || bad "refused add leaves nothing"

newroot 5
mkdir -p "$R/var/db/pkg/base"; printf 'F 0755 %s usr/local/bin/hello\n' "$(printf x | shasum -a 256 | cut -d' ' -f1)" >"$R/var/db/pkg/base/MANIFEST"
expect "add refuses a file owned by base" 1 "$PKG" --root "$R" add --local "$W/hello.upkg"
has "conflict names base" "owned by base"

newroot 6
"$PKG" --root "$R" add --local "$W/hello.upkg" >/dev/null 2>&1
mkdir -p "$W/s2/usr/local/bin"; echo other >"$W/s2/usr/local/bin/hello"
"$MKPKG" build "$W/other.upkg" other 1.0 "$ARCH" "$W/s2" && "$MKPKG" sign "$W/good.key" "$W/other.upkg"
expect "add refuses a file owned by another package" 1 "$PKG" --root "$R" add --local "$W/other.upkg"
has "conflict names the owner" "owned by hello"
mkdir -p "$W/s3/usr/local/bin"; echo three >"$W/s3/usr/local/bin/three"
"$MKPKG" build "$W/c.upkg" c 1.0 "$ARCH" "$W/s3" "conflicts: hello" && "$MKPKG" sign "$W/good.key" "$W/c.upkg"
expect "add refuses a declared conflict" 1 "$PKG" --root "$R" add --local "$W/c.upkg"
"$MKPKG" build "$W/d.upkg" d 1.0 "$ARCH" "$W/s3" "depends: hello >= 1.0" && "$MKPKG" sign "$W/good.key" "$W/d.upkg"
expect "add with a satisfied dependency works" 0 "$PKG" --root "$R" add --local "$W/d.upkg"
expect "del refuses a package another needs" 1 "$PKG" --root "$R" del hello
expect "del --force overrides the dependency check" 0 "$PKG" --root "$R" --force del hello
newroot 7
"$MKPKG" build "$W/e.upkg" e 1.0 "$ARCH" "$W/s3" "depends: missing" && "$MKPKG" sign "$W/good.key" "$W/e.upkg"
expect "add refuses an unmet dependency" 1 "$PKG" --root "$R" add --local "$W/e.upkg"
"$MKPKG" build "$W/f.upkg" f 1.0 otherarch "$W/s3" && "$MKPKG" sign "$W/good.key" "$W/f.upkg"
expect "add refuses another architecture" 1 "$PKG" --root "$R" add --local "$W/f.upkg"

# --- failed adds leave nothing behind ---------------------------------------
newroot 8
mkdir -p "$R/usr/local/bin"
BEFORE=$(tree "$R")
MKPKG_BADHASH=1 "$MKPKG" build "$W/badhash.upkg" badhash 1.0 "$ARCH" "$W/s1" && "$MKPKG" sign "$W/good.key" "$W/badhash.upkg"
expect "add refuses a file that does not match its manifest hash" 1 "$PKG" --root "$R" add --local "$W/badhash.upkg"
[ "$(tree "$R")" = "$BEFORE" ] && ok "failed add leaves the tree unchanged" || { bad "failed add leaves the tree unchanged"; tree "$R" | diff - /dev/null | head; }
# a crash mid-install: journal an add by hand, drop a file, and let the next run roll back
mkdir -p "$R/var/db/pkg" "$R/usr/local/halfway"
echo x >"$R/usr/local/halfway/file"
printf 'add halfway\nD usr/local/halfway\nF usr/local/halfway/file\n' >"$R/var/db/pkg/.journal"
expect "a stale journal is rolled back by the next run" 0 "$PKG" --root "$R" info
"$PKG" --root "$R" del nothere >/dev/null 2>&1
[ ! -e "$R/usr/local/halfway" ] && [ ! -e "$R/var/db/pkg/.journal" ] && ok "interrupted install rolled back" || bad "interrupted install rolled back"

# --- symlink and path safety -------------------------------------------------
newroot 9
mkdir -p "$W/outside"; ln -s "$W/outside" "$R/usr/local/bin"
expect "add refuses to install through a symlinked directory" 1 "$PKG" --root "$R" add --local "$W/hello.upkg"
[ -z "$(ls "$W/outside")" ] && ok "nothing written through the symlink" || bad "nothing written through the symlink"

echo
echo "pkgdev: $((CHECKS - FAILS))/$CHECKS checks passed"
[ $FAILS -eq 0 ]
