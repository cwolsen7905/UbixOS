# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
#include ../../../Makefile.inc
#include ../../Makefile.inc
#include ../Makefile.inc
include ../libc.inc

#Objects
OBJS = _Exit.o a64l.o abort.o atexit.o atof.o atoi.o atol.o atoll.o bsearch.o calloc.o exit.o getenv.o getopt.o getopt_long.o getsubopt.o grantpt.o hcreate.o heapsort.o imaxabs.o imaxdiv.o insque.o l64a.o llabs.o lldiv.o lsearch.o malloc.o merge.o putenv.o qsort.o qsort_r.o radixsort.o rand.o random.o reallocf.o realpath.o remque.o setenv.o strfmon.o strtoimax.o strtol.o strtoll.o strtonum.o strtoq.o strtoul.o strtoull.o strtoumax.o strtouq.o system.o tdelete.o tfind.o tsearch.o twalk.o

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

