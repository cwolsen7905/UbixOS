# $Id: Makefile 89 2016-01-12 00:20:40Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../../Makefile.incl
include ../../Makefile.incl
include ../Makefile.incl

INCLUDES += -I../locale/

#Objects
OBJS = ffsl.o fls.o flsl.o memccpy.o memmem.o stpcpy.o strcasecmp.o strcasestr.o strcoll.o strcspn.o strdup.o strerror.o strlcat.o strlcpy.o strmode.o strncat.o strncpy.o strnstr.o strpbrk.o strsep.o strsignal.o strspn.o strstr.o strtok.o strxfrm.o wcscat.o wcscoll.o wcscpy.o wcscspn.o wcslcat.o wcslcpy.o wcsncat.o wcsncmp.o wcsncpy.o wcspbrk.o wcsrchr.o wcsspn.o wcsstr.o wcstok.o wcswidth.o wcsxfrm.o wmemcmp.o wmemcpy.o wmemmove.o wmemset.o

all: $(OBJS)

# Compile the source files
.cc.o:
	$(CXX) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -c -o $@ $<
 
.cc.s:
	$(CXX) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -S -o $@ $<
 
.c.o:
	$(CC) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -c $<

.c.s:
	$(CC) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -S -o $@ $<
 
.S.o:
	$(CC) $(CFLAGS) -Wall -nostdlib -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS)

