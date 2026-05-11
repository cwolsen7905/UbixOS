#  (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../../Makefile.incl
include ../../Makefile.incl

#  Objects
OBJS = fat.o fat_access.o fat_cache.o fat_filelib.o fat_format.o fat_fread.o fat_misc.o fat_string.o fat_table.o fat_write.o

all: $(OBJS)

#  Compile Types
.cc.o:
	$(CXX) ${CFLAGS} ${INCLUDES} -c -o $@ $<
.c.o:
	$(CC)  ${CFLAGS} ${INCLUDES} -c -o $@ $<
.S.o:
	$(CC)  ${CFLAGS} ${INCLUDES} -c -o $@ $<

# Clean up the junk
clean:
	$(REMOVE) $(OBJS)

.for _d in ${OBJS:.o=.d}
.sinclude "${_d}"
.endfor