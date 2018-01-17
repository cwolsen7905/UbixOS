#  (C) 2002 The UbixOS Project
#  $Id: Makefile 150 2016-01-18 19:59:43Z reddawg $

# Include Global 'Source' Options
include ../../Makefile.incl
include ../Makefile.incl

#  Objects
OBJS = vfs_calls.o tty.o kern_sig.o pipe.o descrip.o kern_sysctl.o gen_calls.o endtask.o ld.o time.o elf.o ubthread.o vitals.o access.o syscall.o syscall_posix.o syscalls_posix.o execve.o
#OBJS += ../${_ARCH}/schedyield.o ../${_ARCH}/kpanic.o ../${_ARCH}/timer.o ../${_ARCH}/spinlock.o ../${_ARCH}/exec.o ../${_ARCH}/sys_call_new.o ../${_ARCH}/sys_call.o ../${_ARCH}/bioscall.o ../${_ARCH}/fork.o ../${_ARCH}/syscall.o ../${_ARCH}/systemtask.o ../${_ARCH}/sched.o ../${_ARCH}/cpu.o 
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
