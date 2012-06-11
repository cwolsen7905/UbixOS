CC = gcc

REMOVE = rm -fr
.if defined(PICA)
CFLAGS = -O2 -fno-strict-aliasing -pipe  -D__DBINTERFACE_PRIVATE -DINET6 -DPOSIX_MISTAKE -DBROKEN_DES -DPORTMAP -DDES_BUILTIN -DYP -DHESIOD -Wsystem-headers -Werror -Wall -Wno-format-y2k -Wno-uninitialized -nostdinc -nostdlib -fpic -DPIC
CFLAGS_UTHREAD = -DPIC -O2 -fno-strict-aliasing -pipe  -DPTHREAD_KERNEL -D_LOCK_DEBUG -D_PTHREADS_INVARIANTS
.else
CFLAGS = -fpic -DPIC -O2 -fno-strict-aliasing -pipe -D__DBINTERFACE_PRIVATE -DINET6 -DPOSIX_MISTAKE -DBROKEN_DES -DPORTMAP -DDES_BUILTIN -DYP -DHESIOD -Wsystem-headers -Wall -Wno-format-y2k -Wno-uninitialized
CFLAGS_UTHREAD = -nostdinc -nostdlib -fpic -DPIC -O2 -fno-strict-aliasing -pipe  -DPTHREAD_KERNEL -D_LOCK_DEBUG -D_PTHREADS_INVARIANTS
.endif

INCLUDES = -I${LIBPATH}/src/include.new -I${LIBPATH}/src/lib/libc/include -I${LIBPATH}/src/lib/libc/i386 -I${LIBPATH}/src/contrib/gdtoa -I${LIBPATH}/src/lib/libc  -I${LIBPATH}/src/lib/libc/resolv -I${LIBPATH}/src/lib/libc/locale -I${LIBPATH}/src/lib/libc/rpc

INCLUDES_UTHREAD = -I${LIBPATH}/src/lib/libc/include -I${LIBPATH}/src/include.new
