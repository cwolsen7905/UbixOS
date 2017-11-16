# $Id: Makefile 89 2016-01-12 00:20:40Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../../../Makefile.inc
include ../../../Makefile.inc
include ../Makefile.inc

INCLUDES += -I../

#Objects
OBJS = Ovfork.o brk.o cerror.o exect.o getcontext.o i386_clr_watch.o i386_get_fsbase.o i386_get_gsbase.o i386_get_ioperm.o i386_get_ldt.o i386_set_fsbase.o i386_set_gsbase.o i386_set_ioperm.o i386_set_ldt.o i386_set_watch.o i386_vm86.o pipe.o ptrace.o reboot.o sbrk.o setlogin.o sigreturn.o syscall.o

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
	$(CC) $(CFLAGS) -Wall -nostdlib $(INCLUDES) -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS)

