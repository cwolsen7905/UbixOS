# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../../Makefile.incl
include ../../Makefile.incl

#Objects
OBJS = opendir.o readdir.o closedir.o

#Output
OUTPUT = dirent.so

$(OUTPUT): $(OBJS)

# Compile the source files
.cc.o:
	$(CXX) ${CFLAGS} -Wall -nostdinc -O -I../include -c -o $@ $<

.cc.s:
	$(CXX) ${CFLAGS} -Wall -nostdinc -O -I../include -S -o $@ $<

.c.o:
	$(CC) ${CFLAGS} -Wall -nostdinc -O -I../include -I../../../include_old -c $<

.c.s:
	$(CC) ${CFLAGS} -Wall -nostdinc -O -I../include -S -o $@ $<

.S.o:
	$(CC) ${CFLAGS} -Wall -nostdinc -c -o $@ $<

# Clean up the junk
clean:
	$(REMOVE) $(OBJS) $(OUTPUT)
