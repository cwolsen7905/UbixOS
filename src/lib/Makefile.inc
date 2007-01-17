# $Id$
# 'lib' options

LDFLAGS = -Bshareable
#CFLAGS = -fpic -DPIC -O2 -fno-strict-aliasing -pipe -D__DBINTERFACE_PRIVATE -DPOSIX_MISTAKE -DBROKEN_DES -DPORTMAP -DDES_BUILTIN -DYP -DHESIOD -Wsystem-headers -Wall -Wno-format-y2k -Wno-uninitialized
CFLAGS = -fno-builtin -Wno-uninitialized -O2 -fno-strict-aliasing -pipe 

INCLUDES = -I../include -I../../../include -I../i386 -I../include
