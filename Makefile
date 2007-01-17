# $Id$
# The System Makefile (C) 2002 The UbixOS Project

all: kernel csu ubix_api libc_old libc ubix libcpp bin tools
# depend kernel tools

csu: src
	(cd src/lib/csu;make)

ubix_api: src
	(cd src/lib/ubix_api;make)

libc_old: src
	(cd src/lib/libc_old;make)

libc: src
	(cd src/lib/libc;make)

objgfx40: src
	(cd src/lib/objgfx40;make)

views: src
	(cd src/lib/views/sunlight;make)

ubix: src
	(cd src/lib/ubix;make)

libcpp: src
	(cd src/lib/libcpp;make)

depend: src
	(cd src/lib/ubix;make)

bin: src
	(cd src/bin;make)

libstdc++: src
	(cd src/lib/libstdc++;make)

kernel: src
	(cd src/sys;make)

tools: src
	(cd src/tools;make)

install:
	(cd src/sys;make install)
	
clean:
	(cd src/sys;make clean)
	(cd src/lib/csu;make clean)
	(cd src/lib/ubix_api;make clean)
	(cd src/lib/libc_old;make clean)
	(cd src/lib/libc;make clean)
	(cd src/bin;make clean)
	(cd src/lib/ubix;make clean)
	(cd src/lib/objgfx40;make clean)
	(cd src/lib/libcpp;make clean)
	(cd src/lib/views/sunlight;make clean)
	(cd src/lib/libstdc++;make clean)
