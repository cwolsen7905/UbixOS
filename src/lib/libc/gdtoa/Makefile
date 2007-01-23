# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
#include ../../../Makefile.inc
#include ../../Makefile.inc
#include ../Makefile.inc
include ../libc.inc

INCLUDES += -I../locale/ -I../../../contrib/gdtoa -I../i386
CFLAGS = -fpic -DPIC -O2 -fno-strict-aliasing -pipe  -D__DBINTERFACE_PRIVATE -DINET6 -DPOSIX_MISTAKE -DBROKEN_DES -DPORTMAP -DDES_BUILTIN -DYP -DHESIOD

#Objects
#OBJS = _hdtoa.o _ldtoa.o dmisc.o dtoa.o g_Qfmt.o g__fmt.o g_ddfmt.o g_dfmt.o g_ffmt.o g_xLfmt.o g_xfmt.o gdtoa.o gethex.o glue.o gmisc.o hd_init.o hexnan.o machdep_ldisx.o misc.o smisc.o strtoIQ.o strtoId.o strtoIdd.o strtoIf.o strtoIg.o strtoIx.o strtoIxL.o strtod.o strtodI.o strtodg.o strtof.o strtopQ.o strtopd.o strtopdd.o strtopf.o strtopx.o strtopxL.o strtorQ.o strtord.o strtordd.o strtorf.o strtorx.o strtorxL.o sum.o ulp.o #arithchk.o
OBJS = _hdtoa.o _ldtoa.o glue.o dmisc.o dtoa.o gdtoa.o gethex.o gmisc.o hd_init.o hexnan.o misc.o smisc.o  strtod.o strtodg.o strtof.o strtord.o sum.o ulp.o machdep_ldisx.o strtopx.o

all: $(OBJS)

# Compile the source files
.cc.o:
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<
 
.cc.s:
	$(CXX) $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

.c.s:
	$(CC) $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.S.o:
	$(CC) $(CFLAGS) -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS)

