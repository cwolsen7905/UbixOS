# bin/ubdisk/Makefile — list block devices + partitions (the `diskutil list` analog).
include ../../Makefile.incl
include ../Makefile.incl

BINARY = ubdisk
OBJS   = main.o

EXTRA_CFLAGS = -I${SRCTOP}/include
EXTRA_LIBS   = -L${OBJ_DIR}/lib -lubix_api   # ubix_disk_query (native syscall 68)

.include "${UBIX_MK}/ubix.musl.prog.mk"
