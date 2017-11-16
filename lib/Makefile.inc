# $Id: Makefile.inc 103 2016-01-12 05:32:24Z reddawg $
# 'lib' options

LDFLAGS = -Bshareable
#CFLAGS = -fpic -DPIC -O2 -fno-strict-aliasing -pipe -D__DBINTERFACE_PRIVATE -DPOSIX_MISTAKE -DBROKEN_DES -DPORTMAP -DDES_BUILTIN -DYP -DHESIOD -Wsystem-headers -Wall -Wno-format-y2k -Wno-uninitialized
CFLAGS = -fno-builtin -Wno-uninitialized -O2 -fno-strict-aliasing -pipe 

INCLUDES = -I../include -I../../../include -I../i386 -I../include

INCLUDES = -I${INCLUDE}
CFLAGS = ${WFLAGS} -Wall -nostdlib -nostdinc -fno-builtin -fno-exceptions -O
