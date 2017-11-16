# $Id: Makefile.inc 113 2016-01-14 00:40:12Z reddawg $

INCLUDES = -I${INCLUDE}
CFLAGS = ${KFLAGS} -Wall -nostdlib -nostdinc -fno-builtin -fno-exceptions -ffreestanding -O
