# $Id: Makefile 148 2016-01-18 19:34:32Z reddawg $
# Application Makefile (C) 2002-2004 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.inc
include ../Makefile.inc

#Linker
LD = ld

#Binary File Name
BINARY = init

#Delete Program
REMOVE = rm -f

#Objects
OBJS = main.o

#LIBRARIES = ../../lib/build/libc_old.static.so

# Link The Binary
$(BINARY) : $(OBJS)
	$(CC) $(CFLAGS) -static -o $(BUILD_DIR)/$@ $(STARTUP) $(LIBRARIES) $(OBJS)
#	strip $(BINARY)

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
