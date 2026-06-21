#!/bin/sh
# build.sh — cross-compile bmake from $WRKSRC with the uBixOS musl world
# toolchain.  Mirrors the object set of configure's make-bootstrap.sh, but uses
# the cross CC/CFLAGS and links like a uBixOS world program (musl crt + ld.so).
# Invoked by tools/ports/bmake/Makefile do-build.  See README.md.
set -e
: "${WRKSRC:?}" "${CC:?}" "${BUILD:?}" "${MUSL_ARCH:?}"

cd "$WRKSRC"
# No USE_META / filemon: meta-mode build tracking needs kernel filemon support
# uBixOS lacks; plain bmake is what self-hosting needs.
CFLAGS="$PORT_CFLAGS -I. -I$WRKSRC -DHAVE_CONFIG_H -DMAKE_NATIVE -DBMAKE_PATH_MAX=1024"

objs=""

# main/job carry the machine/version/sys-path string macros.
for o in main job; do
	echo "  cc $o.c"
	$CC $CFLAGS \
	  -DMAKE_VERSION='"20240212"' \
	  -DMACHINE="\"$MUSL_ARCH\"" -DMACHINE_ARCH="\"$MUSL_ARCH\"" \
	  -D_PATH_DEFSYSPATH='"/usr/share/mk"' \
	  -c -o "$o.o" "$o.c"
	objs="$objs $o.o"
done

# core + the one compat shim (stresep) this config needs.
for o in arch buf compat cond dir for hash lst make make_malloc metachar \
         parse sigcompat str suff targ trace var util stresep; do
	echo "  cc $o.c"
	$CC $CFLAGS -c -o "$o.o" "$o.c"
	objs="$objs $o.o"
done

echo "  link bmake"
mkdir -p "$BUILD/bin"
$CC -nostdlib \
  "$BUILD/obj/musl/lib/Scrt1.o" "$BUILD/obj/musl/lib/crti.o" \
  $objs \
  -L"$BUILD/lib" -lc \
  "$BUILD/obj/musl/lib/crtn.o" \
  -Wl,-dynamic-linker,/lib/ld-musl-"$MUSL_ARCH".so.1 -Wl,-rpath,/lib -pie \
  -o "$BUILD/bin/bmake"
echo "==> built $BUILD/bin/bmake"
