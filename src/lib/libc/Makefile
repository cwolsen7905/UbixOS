# $Id: Makefile 89 2016-01-12 00:20:40Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.inc
include ../Makefile.inc

#Objects
OBJS =

AR = ar

LPIC = pic.a

#Sub Sections
SUBS = ./*/*.o ./db/*/*.o ./i386/*/*.o

#Output
OUTPUT = libc.so

lib.so: $(OBJS)
	(cd yp;make)
	(cd xdr;make)
	(cd uuid;make)
	(cd uthread;make)
	(cd sys;make)
	(cd string;make)
	(cd stdtime;make)
	(cd stdlib;make)
	(cd stdio;make)
	(cd rpc;make)
	(cd resolv;make)
	(cd regex;make)
	(cd quad;make)
	(cd posix1e;make)
	(cd nls;make)
	(cd net;make)
	(cd nameser;make)
	(cd locale;make)
	(cd isc;make)
	(cd inet;make)
	(cd i386;make)
	(cd gmon;make)
	(cd gen;make)
	(cd gdtoa;make)
	(cd db;make)
	(cd compat-43;make)
	$(CC) -nostdlib -shared -Wl,-soname,libc.so -o $(OUTPUT) $(OBJS) $(SUBS)

pic.a: $(OBJS)
	$(AR) cq $(LPIC) $(OBJS) $(SUBS)
	ranlib $(LPIC)

# Compile the source files
.cc.o:
	$(CXX)  -Wall -nostdinc -O -I./include -c -o $@ $<
 
.cc.s:
	$(CXX)  -Wall -nostdinc -O -I./include -S -o $@ $<
 
.c.o:
	$(CC)  -Wall -nostdinc -O -I./include -c $<

.c.s:
	$(CC)  -Wall -nostdinc -O -I./include -S -o $@ $<
 
.S.o:
	$(CC)  -Wall -nostdinc -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS) $(OUTPUT)
	(cd yp;make clean)
	(cd xdr;make clean)
	(cd uuid;make clean)
	(cd uthread;make clean)
	(cd sys;make clean)
	(cd string;make clean)
	(cd stdtime;make clean)
	(cd stdlib;make clean)
	(cd stdio;make clean)
	(cd rpc;make clean)
	(cd resolv;make clean)
	(cd regex;make clean)
	(cd quad;make clean)
	(cd posix1e;make clean)
	(cd nls;make clean)
	(cd net;make clean)
	(cd nameser;make clean)
	(cd locale;make clean)
	(cd isc;make clean)
	(cd inet;make clean)
	(cd i386;make clean)
	(cd gmon;make clean)
	(cd gen;make clean)
	(cd gdtoa;make clean)
	(cd db;make clean)
	(cd compat-43;make clean)
