# $Id: Makefile 96 2016-01-12 01:51:45Z reddawg $
# Kernel Makefile (C) 2002 The UbixOS Project

#all: ubixfs-code pci-code ufs-code vfs-code isa-code kernel-code lib-code vmm-code sys-code init-code devfs-code mpi-code kmods-code kernel-img
#ubixfs-code

all: ubixfs-code devfs-code ufs-code vfs-code common-code fat-code

ubixfs-code: ubixfs
	(cd ubixfs;make)

devfs-code: devfs
	(cd devfs;make)

vfs-code: vfs
	(cd vfs;make)

common-code: common 
	(cd common;make)

ufs-code: ufs
	(cd ufs;make)

fat-code: fat
	(cd fat;make)
clean:
	(cd ubixfs;make clean)
	(cd vfs;make clean)
	(cd devfs;make clean)
	(cd ufs;make clean)
	(cd common;make clean)
	(cd fat; make clean)
