# $Id$
# Kernel Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../Makefile.inc

CC=gcc

#Kernel File Name
#BINARY = format

#Objects
OBJS = format.o

# Link the kernel statically with fixed text+data address @1M
$(BINARY) : $(OBJS)
	$(CC)  -o $@ $(OBJS)

# Compile the source files
.cc.o:
	$(CXX)  -Wall -fomit-frame-pointer -O -I../sys/include -c -o $@ $<
 
.cc.s:
	$(CXX)  -Wall -fomit-frame-pointer -O -I../sys/include -S -o $@ $<
 
.c.o:
	$(CC)  -Wall -O -c -o $@ $<
 
.c.s:
	$(CC)  -Wall -fomit-frame-pointer -O -I../sys/include -S -o $@ $<
 
.S.o:
	$(CC)  -Wall -fomit-frame-pointer -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS) $(BINARY) *.core

install-ubix:
	(cp ../sys/compile/ubix.elf /mnts/ubix/boot/kernel/kernel)
	(cp ../bin/init/init /mnts/ubix/bin)
	(cp ../bin/login/login /mnts/ubix/bin)
	(cp ../bin/shell/shell /mnts/ubix/bin)
	(cp ../bin/clock/clock /mnts/ubix/bin)
	(cp ../bin/fdisk/fdisk /mnts/ubix/bin)
	#(cp ../bin/edit/edit /mnts/ubix/bin)
	(cp ../bin/cat/cat /mnts/ubix/bin)
	(cp ../bin/ld/ld.so /mnts/ubix/lib)
	#(cp ../bin/rtld-elf/ld-elf.so.1 /mnts/ubix/libexec)
	(cp ../bin/tcc/tcc /mnts/ubix/bin)
	(cp ../lib/libc_old/libc_old.so /mnts/ubix/lib)
	(cp ../lib/libc/libc.so /mnts/ubix/lib)
	(cp ../lib/ubix_api/ubix_api.so /mnts/ubix/lib)
	(cp ./userdb /mnts/ubix/etc)
	(cp ./motd /mnts/ubix/etc)

format-dsk:
	(cp ../sys/compile/ubix.elf ./)
	(cp ../sys/compile/ubix.elf /ubixos)
	(cp ../bin/init/init ./)
	(cp ../bin/stat/stat ./)
	(cp ../bin/login/login ./)
	(cp ../bin/shell/shell ./)
	(cp ../bin/ls/ls ./)
	(cp ../bin/clock/clock ./)
	(cp ../lib/libc_old/libc_old.so ./)
	(cp ../bin/fdisk/fdisk ./)
	(cp ../bin/cp/cp ./)
	#(cp ../bin/format/format ./)
	(cp ../bin/ubistry/ubistry ./)
	(cp ../sys/boot/mbr/mbr ./)
	(cp ../bin/ld/ld.so ./)
	#(cp /lib/libc.so.5 ./)
	(cp ../bin/ttyd/ttyd ./)
	(./format 50 2000 ${FD_DEVICE} ubix.elf 0 login 3754 init 3754 stat 3754 ubistry 3754 shell 3754 userdb 3754 ls 3754 motd 3754 fdisk 3754 cp 3754 clock 3754 libc_old.so 3754 ld.so 3754 ttyd 3754 )
#	(./format 50 7000 ${FD_DEVICE} ubix.elf 0 login 3754 init 3754 ubistry 3754 shell 3754 userdb 3754 motd 3754 libc_old.so 3754 ld.so 3754 test 3754 libc.so.5 3754)

	#(./format 263 204361 /dev/md1 ubix.elf 0 format 3754 fdisk 3754 ROM8X14.DPF 3754 init 3754 login 3754 shell 3754 userdb 3754 ls 3754 motd 3754 cp 3754)
	#(./format 1064 2000 ${FD_DEVICE} shell 3754 clock 3754)
	#(./format 200 2000 /dev/md1 ubix.elf 0 shell 3754 motd 3754 libc_old.so 3754)
	(rm -fr ubix.elf)
	(rm -fr login)
	(rm -fr init)
	(rm -fr shell)
	(rm -fr ls)
	(rm -fr clock)
	(rm -fr libc_old.so)
	(rm -fr cp)
	(rm -fr fdisk)
	#(rm -fr format)
	
	(rm -fr ubistry)
	(rm -fr ld.so)
	(rm -fr libc.so.5)
	(rm -fr ttyd)
