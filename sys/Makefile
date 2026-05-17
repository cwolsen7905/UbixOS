# Makefile (C) 2002 The UbixOS Project

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

kernel-build: init-code sys-code arch-code kernel-code vmm-code mpi-code fs-code lib-code isa-code pci-code usb-code net-code kernel-img

generic-code: generic
	(cd generic;${MAKE})

init-code: init
	(cd init;${MAKE})

arch-code: arch/${_ARCH}
	(cd arch/${_ARCH};${MAKE})
	
kernel-code: kernel
	(cd kernel;${MAKE})
	
isa-code: isa
	(cd isa;${MAKE})

pci-code: pci
	(cd pci;${MAKE})

usb-code: usb
	(cd usb;${MAKE})

sys-code: sys
	(cd sys;${MAKE})

vmm-code: vmm
	(cd vmm;${MAKE})

fs-code: fs
	(cd fs;${MAKE})

graphics-code: graphics
	(cd graphics;${MAKE})

ld-code: ld
	(cd ld;${MAKE})

lib-code: lib
	(cd lib;${MAKE})

net-code: net
	(cd net;${MAKE})

mpi-code: mpi
	(cd mpi;${MAKE})

kmods-code: kmods
	(cd kmods;${MAKE})

kernel-img: compile
	(/bin/echo "/* " > ./compile/null.c)
	(date >> ./compile/null.c)
	(echo $user >> ./compile/null.c)
	(/bin/echo " */" >> ./compile/null.c)
	(cd compile;${MAKE})

install:
#	(cd compile;cp ./ubix.elf /mnts/ubix/boot/loader)
#	(cd boot;${MAKE} install)
#	(cd ../tools/;make format-dsk)
	(cd ../tools/;${MAKE} install-ubix)

install-kernel:
	@echo "install-kernel: use 'bmake install-kernel' from the repo root"
	@echo
	@echo "***************************************************************"
	@echo "Kernel Install For ${_ARCH} Completed On `LC_ALL=C date`"
	@echo "***************************************************************"

clean:
	(cd init;${MAKE} clean)
	(cd arch/${_ARCH};${MAKE} clean)
	(cd sys;${MAKE} clean)
	(cd kernel;${MAKE} clean)
	(cd vmm;${MAKE} clean)
	(cd mpi;${MAKE} clean)
	(cd fs;${MAKE} clean)
	(cd lib;${MAKE} clean)
	(cd isa;${MAKE} clean)
	(cd pci;${MAKE} clean)
	(cd net;${MAKE} clean)
	(cd compile;${MAKE} clean)
	(cd fs/vfs;${MAKE} clean)
	(cd fs/devfs;${MAKE} clean)
	(cd net;${MAKE} clean)
	(cd usb;${MAKE} clean)
	(cd kmods;${MAKE} clean)
	(cd fs/ufs;${MAKE} clean)
	#(cd ../tools/;${MAKE} clean)
