# $Id: Makefile 96 2016-01-12 01:51:45Z reddawg $
# Kernel Makefile (C) 2002 The UbixOS Project

#all: ubixfs-code pci-code ufs-code vfs-code isa-code kernel-code lib-code vmm-code sys-code init-code devfs-code mpi-code kmods-code kernel-img

all: ubixfs-code devfs-code procfs-code ufs-code vfs-code common-code fat-code

ubixfs-code: ubixfs
	(cd ubixfs;${MAKE})

devfs-code: devfs
	(cd devfs;${MAKE})

procfs-code: procfs
	(cd procfs;${MAKE})

vfs-code: vfs
	(cd vfs;${MAKE})

common-code: common 
	(cd common;${MAKE})

ufs-code: ufs
	(cd ufs;${MAKE})

fat-code: fat
	(cd fat;${MAKE})
clean:
	(cd ubixfs;${MAKE} clean)
	(cd vfs;${MAKE} clean)
	(cd devfs;${MAKE} clean)
	(cd procfs;${MAKE} clean)
	(cd ufs;${MAKE} clean)
	(cd common;${MAKE} clean)
	(cd fat;${MAKE} clean)
