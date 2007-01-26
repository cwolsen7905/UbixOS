# $Id$
# Kernel Makefile (C) 2002 The UbixOS Project

all: pci-code ufs-code vfs-code isa-code kernel-code lib-code vmm-code sys-code init-code devfs-code mpi-code kmods-code kernel-img

generic-code: generic
	(cd generic;make)

init-code: init
	(cd init;make)
	
kernel-code: kernel
	(cd kernel;make)
	
isa-code: isa
	(cd isa;make)

pci-code: pci
	(cd pci;make)

sys-code: sys
	(cd sys;make)

vmm-code: vmm
	(cd vmm;make)

ubixfs-code: ubixfs
	(cd ubixfs;make)

devfs-code: devfs
	(cd devfs;make)

graphics-code: graphics
	(cd graphics;make)

ld-code: ld
	(cd ld;make)

lib-code: lib
	(cd lib;make)

sde-code: sde
	(cd sde;make)

vfs-code: vfs
	(cd vfs;make)

net-code: net
	(cd net;make)

mpi-code: mpi
	(cd mpi;make)

ufs-code: ufs
	(cd ufs;make)

kmods-code: kmods
	(cd kmods;make)

kernel-img: compile
	(/bin/echo "/* " > ./compile/null.c)
	(date >> ./compile/null.c)
	(echo $user >> ./compile/null.c)
	(/bin/echo " */" >> ./compile/null.c)
	(cd compile;make)

install:
#	(cd compile;cp ./ubix.elf /mnts/ubix/boot/loader)
#	(cd boot;make install)
#	(cd ../tools/;make format-dsk)
	(cd ../tools/;make install-ubix)

clean:
	(cd init;make clean)
	(cd compile;make clean)
	(cd sys;make clean)
	(cd vmm;make clean)
	(cd lib;make clean)
	(cd kernel;make clean)
	(cd isa;make clean)
	(cd vfs;make clean)
	(cd ubixfs;make clean)
	(cd pci;make clean)
	(cd sde;make clean)
	(cd devfs;make clean)
	#(cd net;make clean)
	(cd mpi;make clean)
	(cd kmods;make clean)
	(cd ufs;make clean)
	(cd ../tools/;make clean)
