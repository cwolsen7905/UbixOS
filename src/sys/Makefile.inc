# $Id$
# global 'sys' options

INCLUDES = -I../include
CFLAGS = -ggdb -Wall -nostdlib -nostdinc -fno-builtin -fno-exceptions -O -DNOTIMP -DVMMDEBUG #-DLD_DEBUG #-DVFSDEBUG -DDEBUG #-DVMMDEBUG #-DVFSDEBUG
KERNEL = ubix.elf
