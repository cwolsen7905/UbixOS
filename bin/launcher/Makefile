# $Id: Makefile 115 2016-01-14 02:55:19Z reddawg $
# Application Makefile (C) 2002-2004 The UbixOS Project

# Include Global 'Source' Options
Makefile.incl
Makefile.incl

INCLUDES += -I./include -I../../lib/views/sunlight/include -I../../lib/libcpp/include -I../../lib/objgfx40/objgfx40
CFLAGS = -nostdlib -fno-exceptions -frtti
LIBRARIES += ../../lib/libstdc++/*.o

#Linker
LD = ld

#Binary File Name
BINARY = launcher

#Delete Program
REMOVE = rm -f

#Objects
OBJS = launcher.o ubixButton.o ubixDesktop.o

# Link The Binary
$(BINARY) : $(OBJS)
	$(CC) $(CFLAGS) -o ${BUILD_DIR}/bin/$@ $(STARTUP) $(LIBRARIES) $(OBJS)
	strip $(BINARY)

# Compile the source files
.cpp.o:
	$(CXX)  -Wall -O -DNOBOOL $(CFLAGS) $(INCLUDES) -c -o $@ $<

.cc.o:
	$(CXX)  -Wall -O -DNOBOOL $(CFLAGS) $(INCLUDES) -c -o $@ $<

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
	$(REMOVE) $(OBJS) ${BUILD_DIR}/bin/$(BINARY)
