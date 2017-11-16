# $Id: Makefile 115 2016-01-14 02:55:19Z reddawg $
# Application Makefile (C) 2002-2004 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.inc
include ../Makefile.inc

#Linker
LD = ld

#Binary File Name
BINARY = edit

#Delete Program
REMOVE = rm -f

#Objects
OBJS = main.o

INCLUDES = -I../../include.new

LIBRARIES = ../../lib/libc/libc.so

STARTUP = ../../lib/csu/*.o

# Link The Binary
$(BINARY) : $(OBJS)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/$@ $(STARTUP) $(LIBRARIES) $(OBJS)
	#strip $(BINARY)

# Compile the source files
.cc.o:
	$(CXX)  -Wall -O $(CFLAGS) $(INCLUDES) -c -o $@ $<

.cc.s:
	$(CXX)  -Wall -O $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.c.o:
	$(CC)  -Wall -O $(CFLAGS) $(INCLUDES) -c -o $@ $<
 
.c.s:
	$(CC)  -Wall -O $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.S.o:
	$(CC)  -Wall $(CLFAGS) $(INCLUDES) -c -o $@ $<
 
# Clean Up The junk
clean:
	$(REMOVE) $(OBJS) $(BUILD_DIR)/$(BINARY)
