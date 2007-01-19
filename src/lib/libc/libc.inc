CC = cc

REMOVE = rm -fr

.if defined(PICA)
CFLAGS = -O2 -fno-strict-aliasing -pipe  -D__DBINTERFACE_PRIVATE -DINET6 -DPOSIX_MISTAKE -DBROKEN_DES -DPORTMAP -DDES_BUILTIN -DYP -DHESIOD -Wsystem-headers -Werror -Wall -Wno-format-y2k -Wno-uninitialized -nostdinc -nostdlib #-fpic -DPIC
CFLAGS_UTHREAD = -DPIC -O2 -fno-strict-aliasing -pipe  -DPTHREAD_KERNEL -D_LOCK_DEBUG -D_PTHREADS_INVARIANTS
.else
CFLAGS = -fpic -DPIC -O2 -fno-strict-aliasing -pipe  -D__DBINTERFACE_PRIVATE -DINET6 -DPOSIX_MISTAKE -DBROKEN_DES -DPORTMAP -DDES_BUILTIN -DYP -DHESIOD -Wsystem-headers -Werror -Wall -Wno-format-y2k -Wno-uninitialized -nostdinc -nostdlib #-fpic -DPIC
CFLAGS_UTHREAD = -fpic -DPIC -O2 -fno-strict-aliasing -pipe  -DPTHREAD_KERNEL -D_LOCK_DEBUG -D_PTHREADS_INVARIANTS
.endif


INCLUDES = -I/usr/home/reddawg/source/ubixos/src/include.new -I/usr/home/reddawg/source/ubixos/src/lib/libc/include -I/home/reddawg/source/ubixos/src/lib/libc/i386 -I/home/reddawg/source/ubixos/src/contrib/gdtoa -I/home/reddawg/source/ubixos/src/lib/libc  -I/home/reddawg/source/ubixos/src/lib/libc/resolv -I/home/reddawg/source/ubixos/src/lib/libc/locale -I/home/reddawg/source/ubixos/src/lib/libc/rpc

