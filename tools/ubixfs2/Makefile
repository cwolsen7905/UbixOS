# tools/ubixfs2/Makefile — host CLI suite for UbixFS v2 (the test harness).
#
# Pure host build (not the cross toolchain): u2fs operates on an image file
# directly.  -idirafter keeps the host system headers ahead of UbixOS's
# include/ (which has a conflicting machine/signal.h) while still finding
# <fs/ubixfs2/format.h>.  Mirrors the makeuser target in the top Makefile.

TOP    = ../..
CC    ?= cc
CFLAGS ?= -O2 -Wall -Wextra
INCS   = -I${TOP}/lib/ubixfs2_core -idirafter ${TOP}/include

CORE   = ${TOP}/lib/ubixfs2_core/ubixfs2_core.c

all: u2fs

u2fs: u2fs.c ${CORE} ${TOP}/lib/ubixfs2_core/ubixfs2_core.h ${TOP}/include/fs/ubixfs2/format.h
	${CC} ${CFLAGS} ${INCS} u2fs.c ${CORE} -o u2fs

clean:
	rm -f u2fs *.o

.PHONY: all clean
