# bin/install/Makefile — build busybox install (1.36.1) for UbixOS.

include ../../Makefile.incl
include ../Makefile.incl

BB_SRC = ${SRCTOP}/contrib/busybox

BINARY = install

.PATH.c: ${.CURDIR} ${BB_SRC} ${BB_SRC}/coreutils ${BB_SRC}/coreutils/libcoreutils ${BB_SRC}/libbb

EXTRA_CFLAGS = -I${BB_SRC} -I${BB_SRC}/coreutils -I${BB_SRC}/coreutils/libcoreutils -w

OBJS = main.o install.o libbb_stubs.o \
       copy_file.o remove_file.o get_last_path_component.o isdirectory.o \
       make_directory.o safe_strncpy.o cp_mv_stat.o parse_mode.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
