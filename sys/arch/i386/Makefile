#  (C) 2002 The UbixOS Project
#  $Id: Makefile 202 2016-01-23 15:21:35Z reddawg $

# Include Global 'Source' Options
include ../../../Makefile.incl
include ../../Makefile.incl

#  Objects
OBJS = support.o strcpy.o strcmp.o strncmp.o memset.o memcmp.o schedyield.o kpanic.o timer.o spinlock.o i386_exec.o sys_call_posix.o sys_call.o bioscall.o fork.o systemtask.o sched.o cpu.o trap.o bios16code.o
# ap-boot.o smp.o vitals.o(obsolete)

all: $(OBJS)

#  Compile Types
.cc.o:
	$(CXX) -DNOBOOL $(CFLAGS) $(INCLUDES) -c -o $@ $<
.cc.s:
	$(CXX) -DNOBOOL $(CFLAGS) $(INCLUDES) -S -o $@ $<
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<
.c.s:
	$(CC) $(CFLAGS) $(INCLUDES) -S -o $@ $<
.S.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# Clean up the junk
clean:
	$(REMOVE) $(OBJS)
