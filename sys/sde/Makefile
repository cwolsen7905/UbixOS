#  (C) 2002 The UbixOS Project
#  $Id: Makefile 89 2016-01-12 00:20:40Z reddawg $

# Include Global 'Source' Options
include ../../Makefile.incl
include ../Makefile.incl

#  Linker
LINKER = ld

#  Remove
REMOVE = rm -fr

#  Objects
OBJS = objgfx40.o ogFont.o ogPixelFmt.o ogPixCon.o ogDisplay_UbixOS.o sde.o main.o colours.o #ogImage.o

CFLAGS += -fno-rtti -nostdinc++

INCLUDES += -I../../lib/objgfx40
all:
	(cp ../../lib/objgfx40/objgfx40.cpp ./)
	(cp ../../lib/objgfx40/ogFont.cpp ./)
	(cp ../../lib/objgfx40/ogPixCon.cpp ./)
	(cp ../../lib/objgfx40/ogPixelFmt.cpp ./)
	(cp ../../lib/objgfx40/ogImage.cpp ./)
	(make allBuild)

allBuild: $(OBJS)

#  Compile Types
.cpp.o:
	$(CXX) -DNOBOOL -D__UBIXOS_KERNEL__ $(CFLAGS) ${INCLUDES} -c -o $@ $<
.cc.o:
	$(CXX) -DNOBOOL -D__UBIXOS_KERNEL__ $(CFLAGS) -fno-rtti $(INCLUDES) -c -o $@ $<

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
