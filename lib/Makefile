# Makefile (C) 2002 The UbixOS Project

all: ubix_api-code libc-code ubix-code libcpp-code #objgfx-code #csu-code
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

views: src
	(cd src/lib/views/sunlight;$(MAKE))

ubix-code: ubix
	(cd ubix;$(MAKE))

libcpp-code: libcpp
	(cd libcpp;$(MAKE))

bin: src
	(cd src/bin;$(MAKE))

libstdc++: src
	(cd src/lib/libstdc++;$(MAKE))

tools: src
	(cd src/tools;$(MAKE))

install:
	(cd src/sys;make install)
	
clean:
	#(cd src/sys;make clean)
	(cd csu;make clean)
	(cd ubix_api;make clean)
	(cd libc;make clean)
	(cd ubix;make clean)
