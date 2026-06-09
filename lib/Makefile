# Makefile (C) 2002 The uBixOS Project

# aarch64 (Track B port) builds the core libraries the CLI world needs —
# ubix_api (the native-syscall thunks every bin/ program links) — plus objgfx
# (the C++ rendering library) now that the desktop (#5) is being ported, and
# libaudio (a thin ioctl shim that links into vdoom; it runs silent until a
# sound device exists on the virt platform).  bearssl + libpw are now arch-clean
# and build for aarch64 too — bin/authd (the graphical-login credential daemon)
# links libpw + libbearssl, and the desktop spawns it from disk.  libhttp (and
# its wget/httpsget consumers) plus the NetSurf library stack still hardcode
# i386-only build flags and stay i386-only until their Makefiles are
# de-hardcoded.  crt.a is an x86_64-elf libgcc shim (real aarch64 libgcc is
# complete) and is not on the bin/ link line, so it is omitted.
.if ${_ARCH} == "aarch64"
all: ubix_api-code objgfx-code libaudio-code libbearssl-code libpw-code
.else
all: crt-code ubix_api-code ubix-code objgfx-code libaudio-code libbearssl-code libpw-code libhttp-code libwapcaplet-code libparserutils-code libcss-code libhubbub-code libdom-code libnsgif-code libnsbmp-code libnsfb-code libnsutils-code libutf8proc-code #csu-code
.endif
# bin tools
# depend kernel tools

csu-code: csu
	(cd csu;$(MAKE) MACHINE_ARCH=i386 MACHINE_CPUARCH=i386 "CFLAGS+=-I${.CURDIR}/../include -I${.CURDIR}/libc/include")

crt-code: crt
	(cd crt;$(MAKE))

ubix_api-code: ubix_api
	(cd ubix_api;$(MAKE))

objgfx-code: objgfx
	(cd objgfx;$(MAKE))

ubix-code: ubix
	(cd ubix;$(MAKE))


bin: src
	(cd src/bin;$(MAKE))

tools: src
	(cd src/tools;$(MAKE))

install:
	(cd src/sys;${MAKE} install)

libaudio-code: libaudio
	(cd libaudio;$(MAKE))

libbearssl-code: libbearssl
	(cd libbearssl;$(MAKE))

# Must build after libbearssl (links libbearssl.so).
libpw-code: libpw
	(cd libpw;$(MAKE))

# Must build after libbearssl (links libbearssl.so).
libhttp-code: libhttp
	(cd libhttp;$(MAKE))

# NetSurf browser library stack (leaf lib, no deps / no codegen).
libwapcaplet-code: libwapcaplet
	(cd libwapcaplet;$(MAKE))

# NetSurf parser/charset utils (aliases.inc pre-generated + vendored).
libparserutils-code: libparserutils
	(cd libparserutils;$(MAKE))

# NetSurf CSS engine (property parsers pre-generated; links parserutils+wapcaplet).
libcss-code: libcss
	(cd libcss;$(MAKE))

# NetSurf HTML5 parser (entities.inc + gperf element-type pre-generated).
libhubbub-code: libhubbub
	(cd libhubbub;$(MAKE))

# NetSurf DOM (+ hubbub HTML binding; no codegen — JS bindings unused).
libdom-code: libdom
	(cd libdom;$(MAKE))

# NetSurf image decoders (leaf libs).
libnsgif-code: libnsgif
	(cd libnsgif;$(MAKE))
libnsbmp-code: libnsbmp
	(cd libnsbmp;$(MAKE))

# NetSurf framebuffer abstraction (core+ram+32bpp plotters + objGFX backend).
libnsfb-code: libnsfb
	(cd libnsfb;$(MAKE))

# NetSurf core deps: small utils + Unicode (utf8proc).
libnsutils-code: libnsutils
	(cd libnsutils;$(MAKE))
libutf8proc-code: libutf8proc
	(cd libutf8proc;$(MAKE))

clean:
	(cd crt;${MAKE} clean)
	(cd ubix_api;${MAKE} clean)
	(cd ubix;${MAKE} clean)
	(cd objgfx;${MAKE} clean)
	(cd libaudio;${MAKE} clean)
	(cd libbearssl;${MAKE} clean)
	(cd libpw;${MAKE} clean)
	(cd libhttp;${MAKE} clean)
	(cd libwapcaplet;${MAKE} clean)
	(cd libparserutils;${MAKE} clean)
	(cd libcss;${MAKE} clean)
	(cd libhubbub;${MAKE} clean)
	(cd libdom;${MAKE} clean)
	(cd libnsgif;${MAKE} clean)
	(cd libnsbmp;${MAKE} clean)
	(cd libnsfb;${MAKE} clean)
	(cd libnsutils;${MAKE} clean)
	(cd libutf8proc;${MAKE} clean)
