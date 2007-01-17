#  (C) 2002 The UbixOS Project
#  $Id$

# Include Global 'Source' Options
include ../../Makefile.inc
include ../Makefile.inc

#  Objects
OBJS = kern_sig.o pipe.o kern_descrip.o kern_sysctl.o gen_calls.o schedyield.o tty.o sys_call_new.o sys_call.o endtask.o spinlock.o bioscall.o ld.o time.o fork.o syscall.o syscall_new.o elf.o systemtask.o exec.o sched.o kpanic.o ubthread.o timer.o vitals.o
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
