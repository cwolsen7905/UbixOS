# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
#include ../../Makefile.inc
#include ../Makefile.inc
include ./libc.inc

#Objects
OBJS =

AR = ar

LPIC = pic.a

#Sub Sections
SUBS = ./*/*.o ./db/*/*.o ./i386/*/*.o ../csu/*.o

#Output
OUTPUT = libc.so
OUTPUT2 = libc.a

lib.so: $(OBJS)
	(cd yp;make LIBPATH=${LIBPATH})
	(cd xdr;make LIBPATH=${LIBPATH})
	(cd uuid;make LIBPATH=${LIBPATH})
#	(cd uthread;make LIBPATH=${LIBPATH})
	(cd sys;make LIBPATH=${LIBPATH})
	(cd string;make LIBPATH=${LIBPATH})
	(cd stdtime;make LIBPATH=${LIBPATH})
	(cd stdlib;make LIBPATH=${LIBPATH})
	(cd stdio;make LIBPATH=${LIBPATH})
	(cd rpc;make LIBPATH=${LIBPATH})
	(cd resolv;make LIBPATH=${LIBPATH})
	(cd regex;make LIBPATH=${LIBPATH})
	(cd quad;make LIBPATH=${LIBPATH})
	(cd posix1e;make LIBPATH=${LIBPATH})
	(cd nls;make LIBPATH=${LIBPATH})
	(cd net;make LIBPATH=${LIBPATH})
	(cd nameser;make LIBPATH=${LIBPATH})
	(cd locale;make LIBPATH=${LIBPATH})
	(cd isc;make LIBPATH=${LIBPATH})
	(cd inet;make LIBPATH=${LIBPATH})
	(cd i386;make LIBPATH=${LIBPATH})
	(cd gmon;make LIBPATH=${LIBPATH})
	(cd gen;make LIBPATH=${LIBPATH})
	(cd gdtoa;make LIBPATH=${LIBPATH})
	(cd db;make LIBPATH=${LIBPATH})
	(cd compat-43;make LIBPATH=${LIBPATH})
	$(CC) -nostdlib -shared -Wl,-x -o $(OUTPUT) -Wl,-soname,$(OUTPUT) `lorder $(OBJS) $(SUBS) | tsort -q`
	#$(CC) -nostdlib -shared -Wl,-x -o lib-c.so -Wl,-soname,lib-c.so `lorder $(OBJS) $(SUBS) | tsort -q`
	#(rm -fr pic.a;ar cq pic.a ./i386/*/*.o ./stdio/*.o ./stdlib/*.o ./string/*.o ./sys/*.o ./xdr/*.o ./yp/*.o ./locale/*.o ./gen/*.o ; ranlib ./pic.a)
	#(rm pic.a)
	#$(AR) cq $(LPIC) `lorder $(OBJS) $(SUBS) | tsort -q`
	#ranlib $(LPIC)

lib.a: $(OBJS)
#	(make clean)
	(cd yp;make LIBPATH=${LIBPATH} PICA=true)
	(cd xdr;make LIBPATH=${LIBPATH} PICA=true)
	(cd uuid;make LIBPATH=${LIBPATH} PICA=true)
#       (cd uthread;make LIBPATH=${LIBPATH} PICA=true)
	(cd sys;make LIBPATH=${LIBPATH} PICA=true)
	(cd string;make LIBPATH=${LIBPATH} PICA=true)
	(cd stdtime;make LIBPATH=${LIBPATH} PICA=true)
	(cd stdlib;make LIBPATH=${LIBPATH} PICA=true)
	(cd stdio;make LIBPATH=${LIBPATH} PICA=true)
	(cd rpc;make LIBPATH=${LIBPATH} PICA=true)
	(cd resolv;make LIBPATH=${LIBPATH} PICA=true)
	(cd regex;make LIBPATH=${LIBPATH} PICA=true)
	(cd quad;make LIBPATH=${LIBPATH} PICA=true)
	(cd posix1e;make LIBPATH=${LIBPATH} PICA=true)
	(cd nls;make LIBPATH=${LIBPATH} PICA=true)
	(cd net;make LIBPATH=${LIBPATH})
	(cd nameser;make LIBPATH=${LIBPATH})
	(cd locale;make LIBPATH=${LIBPATH})
	(cd isc;make LIBPATH=${LIBPATH})
	(cd inet;make LIBPATH=${LIBPATH})
	(cd i386;make LIBPATH=${LIBPATH})
	(cd gmon;make LIBPATH=${LIBPATH})
	(cd gen;make LIBPATH=${LIBPATH})
	(cd gdtoa;make LIBPATH=${LIBPATH})
	(cd db;make LIBPATH=${LIBPATH})
	(cd compat-43;make LIBPATH=${LIBPATH})
	(rm -fr libc.a)
	$(AR) cq $(LPIC) `lorder $(OBJS) $(SUBS) | tsort -q`
	ranlib $(LPIC)

pic.a: $(OBJS)
	(cd yp;make PICA=true LIBPATH=${LIBPATH})
	(cd xdr;make PICA=true)
	(cd uuid;make PICA=true)
	(cd uthread;make PICA=true)
	(cd sys;make PICA=true)
	(cd string;make PICA=true)
	(cd stdtime;make PICA=true)
	(cd stdlib;make PICA=true)
	(cd stdio;make PICA=true)
	(cd rpc;make PICA=true)
	(cd resolv;make PICA=true)
	(cd regex;make PICA=true)
	(cd quad;make PICA=true)
	(cd posix1e;make PICA=true)
	(cd nls;make PICA=true)
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
#pic_a: $(OBJS)
#	$(AR) cq $(LPIC) $(OBJS) $(SUBS)
#	ranlib $(LPIC)

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
