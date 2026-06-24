#!/bin/sh
# build.sh — cross-compile Dropbear (sshd) for uBixOS with the musl world
# toolchain.  Like the oksh/bmake ports, Dropbear's own ./configure runs
# target compile+LINK tests that fail under the freestanding world toolchain, so
# we supply a hand-authored config.h (+ localoptions.h) and drive the
# compile/link directly here — bypassing configure and the generated Makefiles.
#
# Builds three artifacts into $BUILD/bin:
#   dropbear      the SSH-2 server (NON_INETD daemon)
#   dropbearkey   host-key generator (run once at first boot to seed /etc)
#
# Invoked by Makefile with WRKSRC/CC/BUILD/MUSL_ARCH/PORT_CFLAGS in the env.
set -e
: "${WRKSRC:?}" "${CC:?}" "${AR:?}" "${BUILD:?}" "${MUSL_ARCH:?}" "${PORT_CFLAGS:?}" "${PORTDIR:?}"

cd "$WRKSRC"

# The port worktree (build/ports/dropbear-*) is shared across arches — it is NOT
# arch-homed.  So the two arches must build SERIALLY, never concurrently (a
# parallel aarch64 + x86_64 build writes .o into the same dirs and corrupts
# them).  Clear any stale/partial objects from a previous (possibly crashed or
# other-arch) run so this build starts from a clean object state.
find . -name '*.o' -delete 2>/dev/null || true
rm -f libtommath/libtommath.a libtomcrypt/libtomcrypt.a

# GCC's own freestanding headers (stdarg.h, stddef.h, limits.h) — musl relies on
# the compiler for these.  -I (musl) beats -isystem, so musl's stdint/etc. still
# win; GCC only fills the compiler-owned gaps.
GCC_INC="-isystem $($CC -print-file-name=include)"

# libgcc supplies compiler helper routines (e.g. __udivti3 for libtommath's
# 128-bit division on 64-bit targets).  -nostdlib drops it, so link it back in.
LIBGCC="$($CC -print-libgcc-file-name)"

LTM=libtommath
LTC=libtomcrypt
LTC_INC="-I$LTC/src/headers -I$LTM"

# ── 0. Dropbear options headers (must precede libtom: Dropbear patches
#       libtommath/libtomcrypt to pull in src/dbmalloc.h -> src/options.h) ─────
cp "$PORTDIR/config.h"       src/config.h
cp "$PORTDIR/localoptions.h" src/localoptions.h
sh src/ifndef_wrapper.sh < src/default_options.h > src/default_options_guard.h

# -Isrc + LOCALOPTIONS_H_EXISTS are needed everywhere (dbmalloc.h is pulled into
# the libtom sources via the Dropbear patch).
CFLAGS="$PORT_CFLAGS $GCC_INC -Isrc -DLOCALOPTIONS_H_EXISTS"

# Parallel compile: read a newline-separated list of .c paths on stdin, compile
# each to its sibling .o using $CC + the exported $CFLAGS_CUR.  xargs returns
# non-zero if any job fails, which `set -e` turns into an abort.
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
export CC
pcompile() {
	CFLAGS_CUR="$1" xargs -P "$JOBS" -n1 sh -c \
		'o="${1%.c}.o"; exec $CC $CFLAGS_CUR -c -o "$o" "$1"' _
}

# ── 1. libtommath.a ──────────────────────────────────────────────────────────
echo "==> libtommath"
ltm_srcs=$(echo "$LTM"/*.c)
printf '%s\n' $ltm_srcs | pcompile "$CFLAGS -I$LTM"
rm -f "$LTM/libtommath.a"
$AR rcs "$LTM/libtommath.a" $(echo $ltm_srcs | sed 's/\.c/.o/g')

# ── 2. libtomcrypt.a ─────────────────────────────────────────────────────────
# Compile every src/**/*.c with LTC_SOURCE + LTM_DESC (libtommath backend).
# gmp_desc.c / tfm_desc.c self-disable without -DGMP_DESC/-DTFM_DESC.  The five
# *tab.c files are textually #included into their parent .c (aes.c, twofish.c,
# …), so they must NOT be compiled standalone — exclude them.
echo "==> libtomcrypt"
ltc_srcs=$(find "$LTC/src" -name '*.c' \
	! -name 'aes_tab.c' ! -name 'safer_tab.c' ! -name 'sober128tab.c' \
	! -name 'twofish_tab.c' ! -name 'whirltab.c')
printf '%s\n' $ltc_srcs | pcompile "$CFLAGS -DLTC_SOURCE $LTC_INC"
rm -f "$LTC/libtomcrypt.a"
$AR rcs "$LTC/libtomcrypt.a" $(printf '%s\n' $ltc_srcs | sed 's/\.c$/.o/')

# ── 3. Dropbear server + dropbearkey objects ─────────────────────────────────
# Object lists transcribed from Makefile.in (COMMONOBJS / CLISVROBJS / SVROBJS /
# KEYOBJS).  Disabled features (agentfwd, x11fwd, tcpfwd, dss, pq-kex) still
# compile — their .c bodies #if themselves down to stubs.
COMMONOBJS="dbutil buffer dbhelpers dss bignum signkey rsa dbrandom queue \
	atomicio compat fake-rfc2553 ltc_prng ecc ecdsa sk-ecdsa crypto_desc \
	curve25519 ed25519 sk-ed25519 dbmalloc gensignkey gendss genrsa gened25519"
CLISVROBJS="common-session packet common-algo common-kex common-channel \
	common-chansession termcodes loginrec tcp-accept listener process-packet \
	dh_groups common-runopts circbuffer list netio chachapoly gcm \
	kex-x25519 kex-dh kex-ecdh kex-pqhybrid sntrup761 mlkem768"
SVROBJS="svr-kex svr-auth sshpty svr-authpasswd svr-authpubkey \
	svr-authpubkeyoptions svr-session svr-service svr-chansession \
	svr-runopts svr-agentfwd svr-main svr-x11fwd svr-tcpfwd svr-authpam"
KEYOBJS="dropbearkey"

# -DDROPBEAR_SERVER selects the server build of the common objects (Dropbear's
# Makefile sets this per-program; we build the server + its keygen only).
echo "==> dropbear objects"
for n in $COMMONOBJS $CLISVROBJS $SVROBJS $KEYOBJS; do printf 'src/%s.c\n' "$n"; done \
	| pcompile "$CFLAGS $LTC_INC -DDROPBEAR_SERVER"

obj_paths() { for n in $1; do printf 'src/%s.o ' "$n"; done; }

# ── 5. Link PIE musl binaries ────────────────────────────────────────────────
# Staged into $BUILD/usr/sbin to mirror the target FS layout (POSIX /usr split —
# system daemons + admin tools live in /usr/sbin; see
# docs/design/filesystem-hierarchy-plan.md).  mkimage-arm.sh copies the staging
# tree into the image verbatim.
DEST="$BUILD/usr/sbin"
link() {
	out="$1"; shift
	echo "  link $out"
	$CC -nostdlib \
		"$BUILD/obj/musl/lib/Scrt1.o" "$BUILD/obj/musl/lib/crti.o" \
		"$@" \
		"$LTC/libtomcrypt.a" "$LTM/libtommath.a" \
		-L"$BUILD/lib" -lc "$LIBGCC" \
		"$BUILD/obj/musl/lib/crtn.o" \
		-Wl,-dynamic-linker,/lib/ld-musl-"$MUSL_ARCH".so.1 -Wl,-rpath,/lib -pie \
		-o "$DEST/$out"
}
mkdir -p "$DEST"
link dropbear    $(obj_paths "$COMMONOBJS") $(obj_paths "$CLISVROBJS") $(obj_paths "$SVROBJS")
link dropbearkey $(obj_paths "$COMMONOBJS") $(obj_paths "$KEYOBJS")
echo "==> built $DEST/dropbear + dropbearkey"
