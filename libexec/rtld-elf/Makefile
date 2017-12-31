# $Id: Makefile 115 2016-01-14 02:55:19Z reddawg $
# Application Makefile (C) 2002-2004 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.incl
include ../Makefile.incl

INCLUDES = -I../../include.new -I./

CFLAGS+=        -Wall -DFREEBSD_ELF -DIN_RTLD -DPIC 
CFLAGS = -m32 -O2 -fno-strict-aliasing -pipe  -Wall -DFREEBSD_ELF -DIN_RTLD -I./ -fpic -DPIC -std=gnu99 -Wformat=2 -Wno-format-extra-args -Werror -DDEBUG
#MrOlsen 2016-01-13 CFLAGS = -O2 -fno-strict-aliasing -pipe  -Wall -DFREEBSD_ELF -DIN_RTLD -I./ -fpic -elf -DPIC -std=gnu99 -Wformat=2 -Wno-format-extra-args -Werror -DDEBUG


#Linker
LD = ld

#Binary File Name
OUTPUT = ld-elf.so.1

#Delete Program
REMOVE = rm -f

#Objects
OBJS = rtld_start.o  reloc.o rtld.o rtld_lock.o map_object.o  malloc.o xmalloc.o debug.o libmap.o

LIBRARIES = #../../lib/libc/libc.so


# Link The Binary
$(OUTPUT) : $(OBJS)
#	$(CC) -fpic -nostdlib -shared -Wl,-soname,$(OUTPUT) -e .rtld_start -o $(OUTPUT) $(OBJS) $(LIBRARIES) $(SUBS)
#	$(CC) $(CFLAGS)  -nostdlib -e .rtld_start -elf -shared -Wl,-Bsymbolic -o $(OUTPUT) $(OBJS) -lc_pic
#	$(CC) $(CFLAGS)  -nostdlib -e .rtld_start -elf -shared -Wl,-Bsymbolic -o $(OUTPUT) $(OBJS) ./libc_pic.a #-lc_pic
	$(CC) $(CFLAGS)  -nostdlib -e .rtld_start -elf -shared -Wl,-Bsymbolic -o ../../build/libexec/$(OUTPUT) $(OBJS) ../../lib/libc/pic.a #-lc_pic
#	strip $(OUTPUT)

# Compile the source files
.cpp.o:
	$(CXX)  -Wall -O $(CFLAGS) $(INCLUDES) -c -o $@ $<

.cc.o:
	$(CXX)  -Wall -O $(CFLAGS) $(INCLUDES) -c -o $@ $<

.cc.s:
	$(CXX)  -Wall -O $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.c.o:
	$(CC)  -Wall -O $(CFLAGS) $(INCLUDES) -c -o $@ $<
 
.c.s:
	$(CC)  -Wall -O $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.S.o:
	$(CC)  -Wall $(CFLAGS) $(INCLUDES) -c -o $@ $<
 
# Clean Up The junk
clean:
	$(REMOVE) $(OBJS) $(OUTPUT)
