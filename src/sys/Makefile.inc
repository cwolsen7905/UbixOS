# $Id$
# global 'sys' options

INCLUDES = -I../include
CFLAGS = -Wall -nostdlib -nostdinc -fno-builtin -fno-exceptions -O -DNOTIMP -DLD_DEBUG -D VMMDEBUG #-DVFSDEBUG -DDEBUG #-DVMMDEBUG #-DVFSDEBUG
KERNEL = ubix.elf
