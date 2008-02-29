# 'bin' options

CFLAGS = -nostdlib -nostdinc -fno-builtin -O2

INCLUDES = -I../../include 

#LIBRARIES = ../../lib/libc_old/math/*.o ../../lib/libc_old/quad/*.o ../../lib/libc_old/stdio/*.o ../../lib/libc_old/stdlib/*.o ../../lib/libc_old/sys/*.o ../../lib/libc_old/string/*.o ../../lib/libc_old/locale/*.o ../../lib/libc_old/gen/*.o ../../lib/libc_old/generic/*.o ../../lib/libc_old/*/*.o

LIBRARIES = ../../lib/libc_old/*/*.o

STARTUP = ../../lib/ubix/*.o
