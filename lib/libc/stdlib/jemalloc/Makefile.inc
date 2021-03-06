# $FreeBSD: releng/11.1/lib/libc/stdlib/jemalloc/Makefile.inc 313240 2017-02-04 17:17:38Z ngie $

.PATH: ${LIBC_SRCTOP}/stdlib/jemalloc

JEMALLOCSRCS:= jemalloc.c arena.c atomic.c base.c bitmap.c chunk.c \
	chunk_dss.c chunk_mmap.c ckh.c ctl.c extent.c hash.c huge.c mb.c \
	mutex.c nstime.c pages.c prng.c prof.c quarantine.c rtree.c stats.c \
	tcache.c ticker.c tsd.c util.c witness.c

SYM_MAPS+=${LIBC_SRCTOP}/stdlib/jemalloc/Symbol.map

CFLAGS+=-I${SRCTOP}/contrib/jemalloc/include

.for src in ${JEMALLOCSRCS}
MISRCS+=jemalloc_${src}
CLEANFILES+=jemalloc_${src}
jemalloc_${src}: ${SRCTOP}/contrib/jemalloc/src/${src} .NOMETA
	ln -sf ${.ALLSRC} ${.TARGET}
.endfor

MAN+=jemalloc.3
CLEANFILES+=jemalloc.3
jemalloc.3: ${SRCTOP}/contrib/jemalloc/doc/jemalloc.3 .NOMETA
	ln -sf ${.ALLSRC} ${.TARGET}

MLINKS+= \
	jemalloc.3 malloc.3 \
	jemalloc.3 calloc.3 \
	jemalloc.3 posix_memalign.3 \
	jemalloc.3 aligned_alloc.3 \
	jemalloc.3 realloc.3 \
	jemalloc.3 free.3 \
	jemalloc.3 malloc_usable_size.3 \
	jemalloc.3 malloc_stats_print.3 \
	jemalloc.3 mallctl.3 \
	jemalloc.3 mallctlnametomib.3 \
	jemalloc.3 mallctlbymib.3 \
	jemalloc.3 mallocx.3 \
	jemalloc.3 rallocx.3 \
	jemalloc.3 xallocx.3 \
	jemalloc.3 sallocx.3 \
	jemalloc.3 dallocx.3 \
	jemalloc.3 sdallocx.3 \
	jemalloc.3 nallocx.3 \
	jemalloc.3 malloc.conf.5

.if defined(MALLOC_PRODUCTION)
CFLAGS+=	-DMALLOC_PRODUCTION
.endif
