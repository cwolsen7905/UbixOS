#!/bin/sh
#-
# Copyright (c) 2002-2026 The UbixOS Project.
#
# Cross-build the NetSurf framebuffer browser (build/bin/nsfb) for UbixOS.
#
# NetSurf uses its own GNU-make "buildsystem" with code-generation and
# generated resources, so it cannot be hand-bmake-ified.  This script drives
# that build with the UbixOS cross toolchain (x86_64-elf-gcc -m32 + musl),
# isolates pkg-config to our vendored libraries, then performs the final link
# by hand (NetSurf's own link rule cannot emit a musl/elf_i386 executable).
#
# Prerequisites (built by `bmake world` first):
#   build/lib/lib{wapcaplet,parserutils,css,hubbub,dom,nsgif,nsbmp,nsfb,
#                 nsutils,utf8proc,http,bearssl,z}.so
#   build/lib/ubix_api.a, build/lib/libgcc32.a
#   build/obj/musl/lib/crt{1,i,n}.o, build/obj/musl/obj/include
#
# Output: build/bin/nsfb (ELF32 i386, interp /lib/ld-musl-i386.so.1).
set -e

SRCTOP=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$SRCTOP/build"
NS="$SRCTOP/contrib/netsurf"
NSBUILD="$SRCTOP/contrib/netsurf-buildsystem"
PCDIR="$BUILD/netsurf-pc"
MUSL="$SRCTOP/contrib/musl"
ML="$BUILD/obj/musl/lib"
CROSS=x86_64-elf-
JOBS="${JOBS:-4}"

# GNU make (NetSurf needs it; macOS /usr/bin/make is GNU make).
GMAKE=make
command -v gmake >/dev/null 2>&1 && GMAKE=gmake

echo "==> NetSurf cross-build (nsfb)"

# 0. Build nsgenbind (host tool) — generates the DOM<->JS Duktape bindings -----
# Needs a modern bison (>= 3, for %code); Apple's /usr/bin/bison is 2.3.  Prefer
# a keg-only brew bison if present.  Built once into build/netsurf-tools and put
# on PATH (the NetSurf JS Makefile invokes a bare `nsgenbind`).
NSGENBIND_SRC="$SRCTOP/contrib/netsurf-nsgenbind"
NSGENBIND_BIN="$BUILD/netsurf-tools/nsgenbind"
for b in /opt/homebrew/opt/bison/bin /usr/local/opt/bison/bin; do
	[ -x "$b/bison" ] && BISON_PATH="$b" && break
done
if [ ! -x "$NSGENBIND_BIN" ]; then
	echo "==> Building nsgenbind (host JS-binding generator)"
	( cd "$NSGENBIND_SRC"
	  PATH="${BISON_PATH:+$BISON_PATH:}$PATH" \
	    "$GMAKE" NSSHARED="$NSBUILD" >"$BUILD/nsgenbind-build.log" 2>&1 )
	GEN=$(ls -d "$NSGENBIND_SRC"/build-*/nsgenbind 2>/dev/null | head -1)
	[ -n "$GEN" ] || { echo "ERROR: nsgenbind build failed (see $BUILD/nsgenbind-build.log)" >&2; exit 1; }
	mkdir -p "$BUILD/netsurf-tools"
	cp "$GEN" "$NSGENBIND_BIN"
fi
export PATH="$BUILD/netsurf-tools:$PATH"

# 1. pkg-config .pc files isolated to our vendored libraries -----------------
mkdir -p "$PCDIR"
gen_pc() { # name short includedir requires
	cat > "$PCDIR/$1.pc" <<EOF
Name: $1
Description: vendored for UbixOS NetSurf
Version: 3.9
Requires: $4
Cflags: -I$3
Libs: -L$BUILD/lib -l$2
EOF
}
gen_pc libwapcaplet   wapcaplet   "$SRCTOP/contrib/libwapcaplet/include"   ""
gen_pc libparserutils parserutils "$SRCTOP/contrib/libparserutils/include" ""
gen_pc libcss         css         "$SRCTOP/contrib/libcss/include"         "libwapcaplet libparserutils"
gen_pc libhubbub      hubbub      "$SRCTOP/contrib/libhubbub/include"      "libparserutils"
gen_pc libdom         dom         "$SRCTOP/contrib/libdom/include"         "libwapcaplet libhubbub"
gen_pc libnsgif       nsgif       "$SRCTOP/contrib/libnsgif/include"       ""
gen_pc libnsbmp       nsbmp       "$SRCTOP/contrib/libnsbmp/include"       ""
gen_pc libnsfb        nsfb        "$SRCTOP/contrib/libnsfb/include"        ""
gen_pc libnsutils     nsutils     "$SRCTOP/contrib/libnsutils/include"     ""
gen_pc libutf8proc    utf8proc    "$SRCTOP/contrib/libutf8proc/include"    ""

# Dummy libpng.pc: NETSURF_USE_PNG=YES hard-errors unless pkg-config finds
# "libpng", and that switch is what defines -DWITH_PNG (the PNG-handler
# registration gate).  We decode PNGs with stb_image, not libpng, so this .pc
# carries empty Cflags/Libs — it satisfies --exists and enables WITH_PNG without
# pulling in (or linking) a real libpng.
cat > "$PCDIR/libpng.pc" <<EOF
Name: libpng
Description: stub (UbixOS decodes PNG via stb_image)
Version: 1.6.0
Cflags:
Libs:
EOF

# 2. Environment -------------------------------------------------------------
# Host tools (convert_font/convert_image) need HOST libpng.  Resolve it via the
# host pkg-config FIRST — before we isolate PKG_CONFIG_LIBDIR to our cross libs
# below, which would otherwise hide host packages and leave png.h unfindable.
HOST_PNG_CFLAGS="$(pkg-config --cflags libpng 2>/dev/null || true)"
HOST_PNG_LIBS="$(pkg-config --libs libpng 2>/dev/null || echo -lpng)"

# CFLAGS MUST be exported (not passed as a make arg) so it augments rather than
# clobbers NetSurf's own CFLAGS (which carry the pkg-config includes).
MUSL_INC="-I$MUSL/include -I$BUILD/obj/musl/obj/include -I$MUSL/arch/i386 -I$MUSL/arch/generic"
export PKG_CONFIG_LIBDIR="$PCDIR"
export PKG_CONFIG_PATH="$PCDIR"
export CFLAGS="-m32 -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -nostdinc $MUSL_INC -I$SRCTOP/include -I$SRCTOP/contrib/zlib -I$SRCTOP/contrib/bearssl/inc -I$SRCTOP/contrib/stb -fcommon"
export LDFLAGS="-L$BUILD/lib -lhttp -lbearssl -lz"
export BUILD_CFLAGS="$HOST_PNG_CFLAGS"
export BUILD_LDFLAGS="$HOST_PNG_LIBS"

# 3. Drive the NetSurf buildsystem ------------------------------------------
# NetSurf's own final link cannot emit a musl/elf_i386 executable (it wants
# crt0.o and rejects our 32-bit .so's as "incompatible"); that failure is
# EXPECTED — we do the real link in step 4.  Log the full build and only echo
# genuine compile errors so a clean `bmake world` is not spammed.
LOG="$BUILD/netsurf-build.log"
cd "$NS"
echo "==> Compiling NetSurf (log: $LOG)"
"$GMAKE" -j"$JOBS" -k \
	TARGET=framebuffer \
	NSBUILD="$NSBUILD" \
	CC="${CROSS}gcc" BUILD_CC=cc AR="${CROSS}ar" \
	NETSURF_USE_CURL=NO NETSURF_USE_OPENSSL=NO \
	NETSURF_USE_PNG=YES NETSURF_USE_JPEG=YES NETSURF_USE_WEBP=NO \
	NETSURF_USE_DUKTAPE=YES NETSURF_USE_NSSVG=NO NETSURF_USE_RSVG=NO \
	NETSURF_USE_ROSPRITE=NO NETSURF_USE_HARU_PDF=NO NETSURF_USE_VIDEO=NO \
	NETSURF_USE_BMP=YES NETSURF_USE_GIF=YES \
	NETSURF_FB_FONTLIB=internal NETSURF_FB_FRONTEND=ubix \
	>"$LOG" 2>&1 || true
# Surface only genuine compile errors (a "file.c:line: error" or fatal error),
# not the expected self-link failure (collect2/ld/crt0.o/incompatible .so).
COMPILE_ERRS=$(grep -E "error:|fatal error:" "$LOG" \
	| grep -vE "collect2:|ld returned|cannot find crt0|skipping incompatible" || true)
if [ -n "$COMPILE_ERRS" ]; then
	echo "==> NetSurf compile errors:" >&2
	printf '%s\n' "$COMPILE_ERRS" | head -20 >&2
fi

# 4. Manual link (mirrors share/mk/ubix.musl.cxx.ubix.prog.mk) ---------------
OBJDIR=$(ls -d "$NS"/build/*-framebuffer 2>/dev/null | head -1)
[ -n "$OBJDIR" ] || { echo "ERROR: NetSurf object dir not found under $NS/build" >&2; exit 1; }
OBJS=$(ls "$OBJDIR"/*.o 2>/dev/null)
[ -n "$OBJS" ] || { echo "ERROR: no objects compiled in $OBJDIR" >&2; exit 1; }
LIBGCC=$(${CROSS}gcc -m32 -print-libgcc-file-name)

mkdir -p "$BUILD/bin"
echo "==> Linking nsfb from $OBJDIR"
${CROSS}gcc -m32 -nostdlib \
	-Wl,-m,elf_i386 \
	-Wl,--export-dynamic \
	-Wl,-dynamic-linker,/lib/ld-musl-i386.so.1 \
	-Wl,-rpath,/lib \
	-Wl,-z,noexecstack \
	"$ML/crt1.o" "$ML/crti.o" \
	$OBJS \
	-Wl,--start-group \
	-L"$BUILD/lib" \
	-lcss -ldom -lhubbub -lparserutils -lwapcaplet \
	-lnsgif -lnsbmp -lnsfb -lnsutils -lutf8proc \
	-lhttp -lbearssl -lz \
	"$BUILD/lib/ubix_api.a" -lc "$BUILD/lib/libgcc32.a" "$LIBGCC" \
	-Wl,--end-group \
	"$ML/crtn.o" \
	-o "$BUILD/bin/nsfb"

echo "==> Built $BUILD/bin/nsfb"
file "$BUILD/bin/nsfb" 2>/dev/null || true
