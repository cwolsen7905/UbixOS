#  (C) 2002 The UbixOS Project
#  $Id: Makefile 89 2016-01-12 00:20:40Z reddawg $

# Include Global 'Source' Options
include ../../Makefile.incl
include ../Makefile.incl

#  Objects
OBJS = libcpp.o

#Include

INCLUDE += -I../libcpp/include/
#-I../../include -I./include -I../libc/include

all: $(OBJS)

#  Compile Types
.cpp.o:
	$(CXX) ${CFLAGS} -Wall -DNOBOOL -fno-exceptions -g -c $(INCLUDE) -o $@ $<
.cc.o:
	$(CXX) ${CFLAGS} -Wall -DNOBOOL -D__HMM -nostdinc -fno-builtin -fno-rtti -fno-exceptions -fomit-frame-pointer -O $(INCLUDE) -c -o $@ $<
.cc.s:
	$(CXX) ${CFLAGS} -Wall -fomit-frame-pointer -O $(INCLUDE) -S -o $@ $<
.c.o:
	$(CC)  ${CFLAGS} -Wall -fomit-frame-pointer -O $(INCLUDE) -c -o $@ $<
.c.s:
	$(CC)  ${CFLAGS} -Wall -fomit-frame-pointer -O $(INCLUDE) -S -o $@ $<
.S.o:
	$(CC)  ${CFLAGS} -Wall -fomit-frame-pointer -c -o $@ $<

# Clean up the junk
clean:
	$(REMOVE) $(OBJS)
