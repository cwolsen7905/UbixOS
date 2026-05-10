# The System Makefile (C) 2002, 2017 The UbixOS Project
# Do not override MAKE — let bmake set it to itself so recursive invocations
# stay within bmake rather than falling back to GNU make.

CURDIR=${.CURDIR}

include Makefile.incl

OBJ_DIR?= ${CURDIR}/build

CLEANDIR=clean

WORLD_LIB_SRC=${CURDIR}/lib
WORLD_LIBEXEC_SRC=${CURDIR}/libexec
WORLD_BIN_SRC=${CURDIR}/bin
WORLD_INC="-I${CURDIR}/include_old -I${CURDIR}/lib/objgfx40/ -I${CURDIR}/lib/libcpp/include"
.if defined(CROSS_PREFIX) && !empty(CROSS_PREFIX)
WORLD_FLAGS=_ARCH=${_ARCH} CC="${CROSS_PREFIX}gcc" CXX="${CROSS_PREFIX}g++" AS="${CROSS_PREFIX}as" AR="${CROSS_PREFIX}ar" LD="${CROSS_PREFIX}ld" NM="${CROSS_PREFIX}nm" OBJDUMP= OBJCOPY="${CROSS_PREFIX}objcopy" RANLIB="${CROSS_PREFIX}ranlib"
.else
WORLD_FLAGS=_ARCH=${_ARCH} CC="cc" CXX="c++" AS="as" AR="ar" LD="ld" NM=nm OBJDUMP= OBJCOPY="objcopy" RANLIB=ranlib
.endif

WMAKE=${MAKE} ${WORLD_FLAGS} CROSS_M32="${CROSS_M32}" INCLUDE=${WORLD_INC} BUILD_DIR=${CURDIR}/build

TMP_PATH=${PATH}
ROOT=/ubixos
ROOT_FAT=/ubixos_fat

DISK_IMAGE?=ubixos.img

all: kernel world install-kernel install-world

# Create a single bootable FAT32 disk image (GRUB + kernel + world).
image:
	@sh tools/mkimage.sh ${DISK_IMAGE}

# Boot the disk image in QEMU (primary IDE master, boot from HD).
# Serial output is captured to serial.log for post-mortem inspection.
run:
	qemu-system-i386 -m 256 -drive file=${DISK_IMAGE},format=raw,if=ide,index=0 \
	  -serial file:serial.log -vga std -device pcnet -net user

# Headless debug run: no display, serial goes to stdout.  Ctrl-C to stop.
run-debug:
	qemu-system-i386 -m 256 -drive file=${DISK_IMAGE},format=raw,if=ide,index=0 \
	  -nographic -serial stdio -device pcnet -net user

# Update just the kernel in an existing disk image (faster than full image rebuild).
kernel-to-image:
	mcopy -o -i ${DISK_IMAGE}@@1M sys/compile/kernel ::/boot/kernel/kernel

kernel:
	@cd sys;${MAKE}

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
	cp -pr build/bin/* ${ROOT_FAT}/bin/
	sync
	@echo
	@echo "***************************************************************"
	@echo "Step 2: Copy Libraries"
	@echo "***************************************************************"
	cp -pr build/lib/* ${ROOT}/lib/
	cp -pr build/lib/* ${ROOT_FAT}/lib/
	cp -pr build/libexec/* ${ROOT}/libexec/
	cp -pr build/libexec/* ${ROOT_FAT}/libexec/
	sync
	@echo
	@echo "***************************************************************"
	@echo "Step 3: Copy Etc"
	@echo "***************************************************************"
	cp -pr etc/* ${ROOT}/etc/
	cp -pr etc/* ${ROOT_FAT}/etc/
	sync
	@echo
	@echo "***************************************************************"
	@echo "World Install For ${_ARCH} Completed On `LC_ALL=C date`"
	@echo "***************************************************************"
	umount /ubixos_fat
	mount /ubixos_fat

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
