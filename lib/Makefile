# Makefile (C) 2002 The UbixOS Project

all: crt-code ubix_api-code ubix-code objgfx-code libaudio-code libbearssl-code libpw-code #csu-code
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

clean:
	(cd crt;${MAKE} clean)
	(cd ubix_api;${MAKE} clean)
	(cd ubix;${MAKE} clean)
	(cd objgfx;${MAKE} clean)
	(cd libaudio;${MAKE} clean)
	(cd libbearssl;${MAKE} clean)
	(cd libpw;${MAKE} clean)
