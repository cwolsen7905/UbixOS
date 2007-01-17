# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../../Makefile.inc
include ../../Makefile.inc
include ../Makefile.inc

INCLUDES += -I../locale/ -I../../../contrib/gdtoa -I../i386

#Objects
OBJS = _hdtoa.o _ldtoa.o arithchk.o dmisc.o dtoa.o g_Qfmt.o g__fmt.o g_ddfmt.o g_dfmt.o g_ffmt.o g_xLfmt.o g_xfmt.o gdtoa.o gethex.o glue.o gmisc.o hd_init.o hexnan.o machdep_ldisx.o misc.o smisc.o strtoIQ.o strtoId.o strtoIdd.o strtoIf.o strtoIg.o strtoIx.o strtoIxL.o strtod.o strtodI.o strtodg.o strtof.o strtopQ.o strtopd.o strtopdd.o strtopf.o strtopx.o strtopxL.o strtorQ.o strtord.o strtordd.o strtorf.o strtorx.o strtorxL.o sum.o ulp.o

all: $(OBJS)

# Compile the source files
.cc.o:
	$(CXX) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -c -o $@ $<
 
.cc.s:
	$(CXX) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -S -o $@ $<
 
.c.o:
	$(CC) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -c $<

.c.s:
	$(CC) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -S -o $@ $<
 
.S.o:
	$(CC) $(CFLAGS) -Wall -nostdlib -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS)

