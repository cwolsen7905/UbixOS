# bin/ubdisk/Makefile — list block devices + partitions (the `diskutil list` analog).
include ../../Makefile.incl
include ../Makefile.incl

BINARY = ubdisk
OBJS   = main.o

EXTRA_CFLAGS = -I${SRCTOP}/include
EXTRA_LIBS   = ${OBJ_DIR}/lib/ubix_api.a   # ubix_disk_query (native syscall 68)

.include "${UBIX_MK}/ubix.musl.prog.mk"
