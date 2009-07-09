# $Id$
# global 'sys' options

INCLUDES = -I../include
CFLAGS = -Wall -nostdlib -nostdinc -fno-builtin -fno-exceptions -O -DNOTIMP -DLD_DEBUG -DVMMDEBUG #-DVFSDEBUG -DDEBUG #-DVMMDEBUG #-DVFSDEBUG
KERNEL = ubix.elf
