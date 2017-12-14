# $Id: Makefile 129 2016-01-14 14:16:33Z reddawg $
# Application Makefile (C) 2002-2004 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.inc
include ../Makefile.inc

#Linker
LD = ld

#Binary File Name
BINARY = shell

#Delete Program
REMOVE = rm -f

#Objects
OBJS = main.o error.o commands.o exec.o input.o

LIBRARIES = ../../build/lib/libc_old.so ../../build/lib/ubix_api.so

# Link The Binary
$(BINARY) : $(OBJS)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/bin/$@ $(STARTUP) $(LIBRARIES) $(OBJS)
	strip ${BUILD_DIR}/bin/$(BINARY)

# Compile the source files
.cpp.o:
	$(CXX)  -Wall -O $(CFLAGS) $(INCLUDES) -c -o $@ $<

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
	$(REMOVE) $(OBJS) $(BUILD_DIR)/bin/$(BINARY)
