#  (C) 2002 The UbixOS Project
#  $Id: Makefile 134 2016-01-15 14:50:24Z reddawg $

# Include Global 'Source' Options
Makefile.incl
Makefile.incl

#  Objects
OBJS = schedyield.o kpanic.o timer.o spinlock.o i386_exec.o sys_call_new.o sys_call.o bioscall.o fork.o syscall.o systemtask.o sched.o cpu.o 
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
