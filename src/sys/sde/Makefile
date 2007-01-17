#  (C) 2002 The UbixOS Project
#  $Id$

# Include Global 'Source' Options
include ../../Makefile.inc
include ../Makefile.inc

#  Linker
LINKER = ld

#  Remove
REMOVE = rm -fr

#  Objects
OBJS = ogFont.o ogPixelFmt.o ogPixCon.o objgfx40.o ogDisplay_UbixOS.o main.o sde.o
# colours.o

CFLAGS += -fno-rtti

all:
	(cp ../../lib/objgfx40/objgfx40.cpp ./)
	(cp ../../lib/objgfx40/ogFont.cpp ./)
	(cp ../../lib/objgfx40/ogPixCon.cpp ./)
	(cp ../../lib/objgfx40/ogPixelFmt.cpp ./)
	(make allBuild)

allBuild: $(OBJS)

#  Compile Types
.cpp.o:
	$(CXX) -DNOBOOL -D__UBIXOS_KERNEL__ $(CFLAGS) ${INCLUDES} -I../../lib/objgfx40 -c -o $@ $<
.cc.o:
	$(CXX) -DNOBOOL -D__UBIXOS_KERNEL__  $(CFLAGS) ${INCLUDES} -I../../lib/objgfx40 -c -o $@ $<
.cc.s:
	$(CXX) -DNOBOOL -D__UBIXOS_KERNEL__ ${CFLAGS} ${INCLUDES} -S -o $@ $<
.c.o:
	$(CC) ${CFLAGS} ${INCLUDES} -c -o $@ $<
.c.s:
	$(CC) ${CFLAGS} ${INCLUDES} -S -o $@ $<
.S.o:
	$(CC) ${CFLAGS} ${INCLUDES} -c -o $@ $<

# Clean up the junk
clean:
	$(REMOVE) $(OBJS) objgfx40.cpp ogFont.cpp ogPixCon.cpp ogPixelFmt.cpp
