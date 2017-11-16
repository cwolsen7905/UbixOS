# $Id: Makefile 148 2016-01-18 19:34:32Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../../Makefile.inc
include ../../Makefile.inc

#Objects
OBJS = mpi.o startsde.o mmap.o error.o getuptime.o gettime.o getdrives.o setuid.o setgid.o getuid.o getgid.o exec.o getpid.o fork.o pidstatus.o getpage.o sched.o auth.o execve.o

#Output
OUTPUT = sys.so

$(OUTPUT): $(OBJS)

# Compile the source files
.cc.o:
	$(CXX) ${CFLAGS} -Wall -nostdinc -O -I../include -c -o $@ $<
 
.cc.s:
	$(CXX) ${CFLAGS} -Wall -nostdinc -O -I../include -S -o $@ $<
 
.c.o:
	$(CC) ${CFLAGS} -Wall -nostdinc -O -I../include -I../../../include -c $<

.c.s:
	$(CC) ${CFLAGS} -Wall -nostdinc -O -I../include -S -o $@ $<
 
.S.o:
	$(CC) ${CFLAGS} -Wall -nostdinc -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS) $(OUTPUT)
