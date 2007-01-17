CC = cc

REMOVE = rm -fr

CFLAGS = -fpic -DPIC -O2 -fno-strict-aliasing -pipe  -D__DBINTERFACE_PRIVATE -DINET6 -DPOSIX_MISTAKE -DBROKEN_DES -DPORTMAP -DDES_BUILTIN -DYP -DHESIOD -Wsystem-headers -Werror -Wall -Wno-format-y2k -Wno-uninitialized -nostdinc -nostdlib #-fpic


INCLUDES = -I/usr/home/reddawg/source/ubix2/src/include.new -I/usr/home/reddawg/source/ubix2/src/lib/libc/include -I/home/reddawg/source/ubix2/src/lib/libc/i386 -I/home/reddawg/source/ubix2/src/contrib/gdtoa -I/home/reddawg/source/ubix2/src/lib/libc  -I/home/reddawg/source/ubix2/src/lib/libc/resolv -I/home/reddawg/source/ubix2/src/lib/libc/locale -I/home/reddawg/source/ubix2/src/lib/libc/rpc

