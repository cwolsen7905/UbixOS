# Makefile (C) 2002 The UbixOS Project

all: ubix_api-code libc-code ubix-code libcpp-code objgfx-code #csu-code
# bin tools
# depend kernel tools

csu-code: csu
	(cd csu;$(MAKE) MACHINE_ARCH=i386 MACHINE_CPUARCH=i386 "CFLAGS+=-I${.CURDIR}/../include -I${.CURDIR}/libc/include")

ubix_api-code: ubix_api
	(cd ubix_api;$(MAKE))

libc-code: libc
	(cd libc;$(MAKE))

objgfx-code: objgfx
	(cd objgfx;$(MAKE))

ubix-code: ubix
	(cd ubix;$(MAKE))

libcpp-code: libcpp
	(cd libcpp;$(MAKE))

bin: src
	(cd src/bin;$(MAKE))

tools: src
	(cd src/tools;$(MAKE))

install:
	(cd src/sys;${MAKE} install)

clean:
	(cd ubix_api;${MAKE} clean)
	(cd libc;${MAKE} clean)
	(cd ubix;${MAKE} clean)
	(cd libcpp;${MAKE} clean)
	(cd objgfx;${MAKE} clean)
