# The System Makefile (C) 2002, 2017 The UbixOS Project
MAKE=make

CURDIR=${.CURDIR}

OBJ_DIR?= ${CURDIR}/obj

CLEANDIR=clean

WORLD_LIB_SRC=${CURDIR}/lib
WORLD_LIBEXEC_SRC=${CURDIR}/libexec
WORLD_BIN_SRC=${CURDIR}/bin
WORLD_INC="-I${CURDIR}/include_old -I${CURDIR}/lib/objgfx40/"
WORLD_FLAGS=_ARCH=${_ARCH} CC="cc" CXX="c++" AS="as" AR="ar" LD="ld" NM=nm  OBJDUMP= OBJCOPY="objcopy"  RANLIB=ranlib

WMAKE= ${MAKE} ${WORLD_FLAGS} INCLUDE=${WORLD_INC} BUILD_DIR=${CURDIR}/build

TMP_PATH=${PATH}
ROOT=/ubixos

all: kernel world install-kernel install-world
# csu ubix_api libc_old libc ubix libcpp bin tools
# depend kernel tools

kernel:
	@cd sys;make

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
	#cd ${WORLD_LIBEXEC_SRC}; ${WMAKE} ${CLEANDIR}
	#cd ${WORLD_BIN_SRC}; ${WMAKE} ${CLEANDIR}
	@echo
	@echo "***************************************************************"
	@echo "Step 2: Build World Libraries"
	@echo "***************************************************************"
	cd ${WORLD_LIB_SRC}; ${WMAKE} all
	@echo
	@echo "***************************************************************"
	@echo "Step 3: Build World Libexec"
	@echo "***************************************************************"
	cd ${WORLD_LIBEXEC_SRC}; ${WMAKE} all
	@echo
	@echo "***************************************************************"
	@echo "Step 4: Build World Binaries"
	@echo "***************************************************************"
	cd ${WORLD_BIN_SRC}; ${WMAKE} all
	@echo
	@echo "***************************************************************"
	@echo "World Build For ${_ARCH} Completed On `LC_ALL=C date`"
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
	cp -pr build/bin/* ${ROOT}/bin/
	sync
	@echo
	@echo "***************************************************************"
	@echo "Step 2: Copy Libraries"
	@echo "***************************************************************"
	cp -pr build/lib/* ${ROOT}/lib/
	cp -pr build/libexec/* ${ROOT}/libexec/
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
	(cd sys;${MAKE} clean)

install-kernel:
	(cd sys;${MAKE} install-kernel)

install: install-world install-kernel
	
clean:
	(cd sys;${MAKE} clean)
	(cd bin;${WMAKE} clean)
	(cd lib;${WMAKE} clean)
	(cd libexec;${WMAKE} clean)
#	(cd src/lib/ubix;make clean)
#	(cd src/lib/objgfx40;make clean)
#	(cd src/lib/libcpp;make clean)
#	(cd src/lib/views/sunlight;make clean)
#	(cd src/lib/libstdc++;make clean)
