# $Id: Makefile 134 2016-01-15 14:50:24Z reddawg $
# Kernel Makefile (C) 2002 The UbixOS Project

include ../Makefile.incl
include Makefile.incl

all:
	@echo
	@echo "***************************************************************"
	@echo "Kernel Build For ${_ARCH} Started On `LC_ALL=C date`"
	@echo "***************************************************************"
	@echo
	@echo "***************************************************************"
	@echo "Step 1: Clean Kernel"
	@echo "***************************************************************"
	#${MAKE} clean
	@echo
	@echo "***************************************************************"
	@echo "Step 2: Build Kernel Objects"
	@echo "***************************************************************"
	${KMAKE} kernel-build
	@echo
	@echo "***************************************************************"
	@echo "Step 3: Link Kernel Objects"
	@echo "***************************************************************"
	${KMAKE} kernel-build
	@echo
	@echo "***************************************************************"
	@echo "Kernel Build For ${_ARCH} Completed On `LC_ALL=C date`"
	@echo "***************************************************************"

kernel-build: init-code sys-code arch-code kernel-code vmm-code mpi-code fs-code lib-code isa-code pci-code net-code kernel-img

generic-code: generic
	(cd generic;make)

init-code: init
	(cd init;make)

arch-code: ${_ARCH}
	(cd ${_ARCH};make)
	
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

fs-code: fs
	(cd fs;make)

graphics-code: graphics
	(cd graphics;make)

ld-code: ld
	(cd ld;make)

lib-code: lib
	(cd lib;make)

sde-code: sde
	(cd sde;make)

net-code: net
	(cd net;make)

mpi-code: mpi
	(cd mpi;make)

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

install-kernel:
	@echo
	@echo "***************************************************************"
	@echo "Kernel Install For ${_ARCH} Started On `LC_ALL=C date`"
	@echo "***************************************************************"
	@echo
	@echo "***************************************************************"
	@echo "Step 1: Copy Kernel"
	@echo "***************************************************************"
	cp compile/kernel ${ROOT}/boot/kernel/
	sync
	@echo
	@echo "***************************************************************"
	@echo "Kernel Install For ${_ARCH} Completed On `LC_ALL=C date`"
	@echo "***************************************************************"

clean:
	(cd init;make clean)
	(cd ${_ARCH};make clean)
	(cd sys;make clean)
	(cd kernel;make clean)
	(cd vmm;make clean)
	(cd mpi;make clean)
	(cd fs;make clean)
	(cd lib;make clean)
	(cd isa;make clean)
	(cd pci;make clean)
	(cd net;make clean)
	(cd compile;make clean)
	#(cd vfs;make clean)
	#(cd sde;make clean)
	#(cd devfs;make clean)
	#(cd net;make clean)
	#(cd kmods;make clean)
	#(cd ufs;make clean)
	#(cd ../tools/;make clean)
