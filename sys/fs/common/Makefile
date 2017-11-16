#  (C) 2002 The UbixOS Project
#  $Id: Makefile 102 2016-01-12 03:59:34Z reddawg $

# Include Global 'Source' Options
include ../../../Makefile.inc
include ../../Makefile.inc

#  Objects
OBJS = crc32.o gpt.o

#Custom Stuff
CFLAGS +=-DBOOTPROG=\"MrOlsen\" -fno-builtin

all: $(OBJS)

#  Compile Types
.cc.o:
	$(CXX) ${CFLAGS} ${INCLUDES} -c -o $@ $<
.cc.s:
	$(CXX) ${CFLAGS} ${INCLUDES} -S -o $@ $<
.c.o:
	$(CC)  ${CFLAGS} ${INCLUDES} -c -o $@ $<
.c.s:
	$(CC)  ${CFLAGS} ${INCLUDES} -S -o $@ $<
.S.o:
	$(CC)  ${CFLAGS} ${INCLUDES} -c -o $@ $<

# Clean up the junk
clean:
	$(REMOVE) $(OBJS)
