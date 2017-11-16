# $Id: Makefile 148 2016-01-18 19:34:32Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

_ARCH?=  ${MACHINE_ARCH}

MAKE=make

OBJ_DIR?= ${CURDIR}/obj

CLEANDIR=clean

KERNEL_SRC=${CURDIR}/sys
KERNEL_OBJ=${OBJ_DIR}${KERNEL_SRC}
KERNEL_CONF=${KERNEL_SRC}/${_ARCH}/conf
KERNEL_NAME=kernel
KERNEL_FLAGS=_ARCH=${_ARCH} CC="cc" CXX="c++" AS="as" AR="ar" LD="ld" NM=nm OBJDUMP= OBJCOPY="objcopy" RANLIB=ranlib
KERNEL_INC=${KERNEL_SRC}/include


KMAKE= ${MAKE} ${KERNEL_FLAGS} INCLUDE=${KERNEL_INC} KERNEL=${KERNEL_NAME}

WORLD_LIB_SRC=${CURDIR}/lib
WORLD_BIN_SRC=${CURDIR}/bin
WORLD_INC=${CURDIR}/include.new
WORLD_FLAGS=_ARCH=${_ARCH} CC="cc" CXX="c++" AS="as" AR="ar" LD="ld" NM=nm  OBJDUMP= OBJCOPY="objcopy"  RANLIB=ranlib

WMAKE= ${MAKE} ${WORLD_FLAGS} INCLUDE=${WORLD_INC} BUILD_DIR=../build

TMP_PATH=${PATH}
ROOT=/ubixos

all: kernel world install-kernel install-world
# csu ubix_api libc_old libc ubix libcpp bin tools
# depend kernel tools

kernel:
	@echo
	@echo "***************************************************************"
	@echo "Kernel Build For ${_ARCH} Started On `LC_ALL=C date`"
	@echo "***************************************************************"
	@echo
	@echo "***************************************************************"
	@echo "Step 1: Clean Kernel"
	@echo "***************************************************************"
	#cd ${KERNEL_SRC}; ${KMAKE} ${CLEANDIR}
	@echo
	@echo "***************************************************************"
	@echo "Step 2: Build Kernel Objects"
	@echo "***************************************************************"
	cd ${KERNEL_SRC}; ${KMAKE} all
	@echo
	@echo "***************************************************************"
	@echo "Step 3: Link Kernel Objects"
	@echo "***************************************************************"
	cd ${KERNEL_SRC}; ${KMAKE} all
	@echo
	@echo "***************************************************************"
	@echo "Kernel Build For ${_ARCH} Completed On `LC_ALL=C date`"
	@echo "***************************************************************"

world:
	@echo
	@echo "***************************************************************"
	@echo "World Build For ${_ARCH} Started On `LC_ALL=C date`"
	@echo "***************************************************************"
	@echo
	@echo "***************************************************************"
	@echo "Step 1: Clean World"
	@echo "***************************************************************"
	#cd ${WORLD_LIB_SRC}; ${WMAKE} ${CLEANDIR}
	#cd ${WORLD_BIN_SRC}; ${WMAKE} ${CLEANDIR}
	@echo
	@echo "***************************************************************"
	@echo "Step 2: Build World Libraries"
	@echo "***************************************************************"
	cd ${WORLD_LIB_SRC}; ${WMAKE} all
	@echo
	@echo "***************************************************************"
	@echo "Step 3: Build World Binaries"
	@echo "***************************************************************"
	cd ${WORLD_BIN_SRC}; ${WMAKE} all
	@echo
	@echo "***************************************************************"
	@echo "World Build For ${_ARCH} Completed On `LC_ALL=C date`"
	@echo "***************************************************************"

install-kernel:
	@echo
	@echo "***************************************************************"
	@echo "Kernel Install For ${_ARCH} Started On `LC_ALL=C date`"
	@echo "***************************************************************"
	@echo
	@echo "***************************************************************"
	@echo "Step 1: Copy Kernel"
	@echo "***************************************************************"
	cp sys/compile/kernel ${ROOT}/boot/kernel/
	sync
	@echo
	@echo "***************************************************************"
	@echo "Kernel Install For ${_ARCH} Completed On `LC_ALL=C date`"
	@echo "***************************************************************"

install-world:
	@echo
	@echo "***************************************************************"
	@echo "World Install For ${_ARCH} Started On `LC_ALL=C date`"
	@echo "***************************************************************"
	@echo
	@echo "***************************************************************"
	@echo "Step 1: Copy Binaries"
	@echo "***************************************************************"
	cp -pr bin/build/* ${ROOT}/bin/
	sync
	@echo
	@echo "***************************************************************"
	@echo "Step 2: Copy Libraries"
	@echo "***************************************************************"
	cp -pr lib/build/* ${ROOT}/lib/
	sync
	@echo
	@echo "***************************************************************"
	@echo "Step 3: Copy Etc"
	@echo "***************************************************************"
	cp -pr etc/* ${ROOT}/etc/
	sync
	@echo
	@echo "***************************************************************"
	@echo "World Install For ${_ARCH} Completed On `LC_ALL=C date`"
	@echo "***************************************************************"

clean-kernel:
	(cd sys;${KMAKE} clean)

install: install-kernel install-world
	
clean:
	(cd sys;${KMAKE} clean)
	(cd bin;${WMAKE} clean)
	(cd lib;${WMAKE} clean)
#	(cd src/lib/ubix;make clean)
#	(cd src/lib/objgfx40;make clean)
#	(cd src/lib/libcpp;make clean)
#	(cd src/lib/views/sunlight;make clean)
#	(cd src/lib/libstdc++;make clean)
