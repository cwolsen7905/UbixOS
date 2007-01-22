# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
#include ../../../../Makefile.inc
#include ../../../Makefile.inc
#include ../Makefile.inc
include ../../libc.inc

INCLUDES += -I../

#CFLAGS += -fpic -DPIC

#Objects
OBJS = _ctx_start.o _set_tp.o _setjmp.o alloca.o fabs.o flt_rounds.o infinity.o ldexp.o makecontext.o modf.o rfork_thread.o setjmp.o signalcontext.o sigsetjmp.o
#Output
OUTPUT = libc.so

$(OUTPUT): $(OBJS)

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
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS) $(OUTPUT)

