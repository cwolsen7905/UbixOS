# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
#include ../../../../Makefile.inc
#include ../../../Makefile.inc
#include ../Makefile.inc
include ../../libc.inc

#INCLUDES += -I../ -I../../include

#Objects
OBJS = bt_close.o bt_conv.o bt_debug.o bt_delete.o bt_get.o bt_open.o bt_overflow.o bt_page.o bt_put.o bt_search.o bt_seq.o bt_split.o bt_utils.o

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
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS)

