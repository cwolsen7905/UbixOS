#!/bin/sh
# build.sh — cross-compile oksh as uBixOS /bin/sh, with the musl world toolchain.
# oksh's own configure runs compiler/link tests that fail under -nostdlib, so we
# use its host-generated pconfig.h (adapted for musl) + the Makefile's object set
# and compile/link directly.  Built with -DNO_CURSES (no terminfo lib on uBixOS;
# /bin/sh doesn't need line-editing terminal control).  Invoked by Makefile.
set -e
: "${WRKSRC:?}" "${CC:?}" "${BUILD:?}" "${MUSL_ARCH:?}"

cd "$WRKSRC"
# EMACS+VI satisfy oksh's "define one" requirement; NO_CURSES strips the terminfo
# includes + calls (emacs.c/vi.c/var.c guard them on !SMALL && !NO_CURSES).
# -include stdint.h: oksh's sh.h uses int64_t but relies on a system header chain
# to pull <stdint.h> in; under musl-freestanding it doesn't, so force it.
# _GNU_SOURCE: oksh uses BSD types (u_char/u_int/…) musl gates behind it; they
# live in <sys/types.h>, which oksh's header chain doesn't pull in under musl, so
# force-include it (and stdint.h for int64_t).
CFLAGS="$PORT_CFLAGS -I. -D_GNU_SOURCE -include sys/cdefs.h -include sys/types.h -include stdint.h -include sys/file.h -DMAXLOGNAME=32 -DEMACS -DVI -DNO_CURSES -w"

OBJS="alloc asprintf c_ksh c_sh c_test c_ulimit edit emacs eval exec expr \
      history io jobs lex mail main misc path shf syn table trap tree tty var \
      version vi confstr reallocarray siglist signame strlcat strlcpy strtonum \
      unvis vis issetugid"

objs=""
for o in $OBJS; do
	echo "  cc $o.c"
	$CC $CFLAGS -c -o "$o.o" "$o.c"
	objs="$objs $o.o"
done

echo "  link sh"
mkdir -p "$BUILD/bin"
$CC -nostdlib \
  "$BUILD/obj/musl/lib/Scrt1.o" "$BUILD/obj/musl/lib/crti.o" \
  $objs \
  -L"$BUILD/lib" -lc \
  "$BUILD/obj/musl/lib/crtn.o" \
  -Wl,-dynamic-linker,/lib/ld-musl-"$MUSL_ARCH".so.1 -Wl,-rpath,/lib -pie \
  -o "$BUILD/bin/sh"
echo "==> built $BUILD/bin/sh"
