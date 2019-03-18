# $Id: Makefile 89 2016-01-12 00:20:40Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../../Makefile.incl
include ../../Makefile.incl
#Objects
OBJS = swab.o strstr.o strspn.o strsep.o strpbrk.o strnstr.o strncpy.o strncmp.o strncat.o strmode.o strdup.o strcspn.o strcat.o strchr.o stpcpy.o memchr.o memccpy.o strcasecmp.o rindex.o index.o ffs.o bzero.o bcopy.o bcmp.o memmove.o strrchr.o strlcat.o strlcpy.o strerror.o strtok.o memcpy.o memset.o memcmp.o strlen.o strcmp.o strcasestr.o

#Output
OUTPUT = libc.so

$(OUTPUT): $(OBJS)

# Compile the source files
.cc.o:
	$(CXX) $(CFLAGS) -Wall -nostdinc -O -I../include -c -o $@ $<
 
.cc.s:
	$(CXX) $(CFLAGS) -Wall -nostdinc -O -I../include -S -o $@ $<
 
.c.o:
	$(CC) $(CFLAGS) -Wall -nostdinc -O -I./include -I../../../include_old -I../locale -c $<

.c.s:
	$(CC) $(CFLAGS) -Wall -nostdinc -O -I../include -S -o $@ $<
 
.S.o:
	$(CC) $(CFLAGS) -Wall -nostdinc -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS) $(OUTPUT)
