# bin/cp/Makefile — build busybox cp (1.36.1) for UbixOS/i386.

include ../../Makefile.incl
include ../Makefile.incl

BB_SRC = ${SRCTOP}/contrib/busybox

BINARY = cp

.PATH.c: ${.CURDIR} ${BB_SRC} ${BB_SRC}/coreutils ${BB_SRC}/coreutils/libcoreutils ${BB_SRC}/libbb

EXTRA_CFLAGS = -I${BB_SRC} -I${BB_SRC}/coreutils -I${BB_SRC}/coreutils/libcoreutils -w

OBJS = main.o cp.o libbb_stubs.o \
       copy_file.o remove_file.o get_last_path_component.o isdirectory.o \
       make_directory.o safe_strncpy.o cp_mv_stat.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
