# $Id$
# Application Makefile (C) 2002-2004 The UbixOS Project

#Linker
LD = ld

#Binary File Name
BINARY = ubixfs

#Delete Program
REMOVE = rm -f

#Objects
OBJS = main.o vfs.o ubixfs.o btree.o ramdrive.o

#Includes
INCLUDES = -I./include -I./

#CFLAGS

CFLAGS = -O3

all: $(BINARY)

# Link The Binary
$(BINARY) : $(OBJS)
	$(CXX) $(CFLAGS) -o $@ $(OBJS)

# Compile the source files
.cpp.o:
	$(CXX)  -ggdb -W -Wall $(CFLAGS) $(INCLUDES) -c -o $@ $<

.cc.o:
	$(CXX)  -W -Wall $(CFLAGS) $(INCLUDES) -c -o $@ $<

.cc.s:
	$(CXX)  -W -Wall $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.c.o:
	$(CC)  -W -Wall $(CFLAGS) $(INCLUDES) -c -o $@ $<
 
.c.s:
	$(CC)  -W -Wall $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.S.o:
	$(CC)  -Wall $(CLFAGS) $(INCLUDES) -c -o $@ $<
 
# Clean Up The junk
clean:
	$(REMOVE) $(OBJS) $(BINARY) *.core
