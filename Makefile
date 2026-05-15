# The System Makefile (C) 2002-2026 The UbixOS Project
# Do not override MAKE — let bmake set it to itself so recursive invocations
# stay within bmake rather than falling back to GNU make.

CURDIR=${.CURDIR}

include Makefile.incl

OBJ_DIR?= ${CURDIR}/build

CLEANDIR=clean

WORLD_LIB_SRC=${CURDIR}/lib
WORLD_LIBEXEC_SRC=${CURDIR}/libexec
WORLD_BIN_SRC=${CURDIR}/bin
WORLD_INC="-I${CURDIR}/include -I${CURDIR}/lib/objgfx40/ -I${CURDIR}/contrib/libcxxabi/include"
.if defined(CROSS_PREFIX) && !empty(CROSS_PREFIX)
WORLD_FLAGS=_ARCH=${_ARCH} CC="${CROSS_PREFIX}gcc" CXX="${CROSS_PREFIX}g++" AS="${CROSS_PREFIX}as" AR="${CROSS_PREFIX}ar" LD="${CROSS_PREFIX}ld" NM="${CROSS_PREFIX}nm" OBJDUMP= OBJCOPY="${CROSS_PREFIX}objcopy" RANLIB="${CROSS_PREFIX}ranlib"
.else
WORLD_FLAGS=_ARCH=${_ARCH} CC="cc" CXX="c++" AS="as" AR="ar" LD="ld" NM=nm OBJDUMP= OBJCOPY="objcopy" RANLIB=ranlib
.endif

WMAKE=${MAKE} ${WORLD_FLAGS} CROSS_M32="${CROSS_M32}" INCLUDE=${WORLD_INC} BUILD_DIR=${CURDIR}/build

DISK_IMAGE?=ubixos.img

# Mount point where the FAT32 partition appears after bmake mount-image.
# macOS auto-names it from the FAT volume label; override on the command line
# if needed (e.g. bmake mount-image MOUNT_POINT=/mnt/ubixos).
MOUNT_POINT?=/Volumes/UBIXOS

# Temp file used to pass the hdiutil disk device between mount/unmount.
_DEV_FILE=/tmp/.ubixos_dev

# ── Primary targets ──────────────────────────────────────────────────────────

all: kernel world image
kernel:
	@mkdir -p ${OBJ_DIR}/boot ${OBJ_DIR}/obj/sys
	@cd sys;${MAKE}

world:
	@mkdir -p ${OBJ_DIR}/boot ${OBJ_DIR}/bin ${OBJ_DIR}/lib ${OBJ_DIR}/libexec \
	           ${OBJ_DIR}/obj/bin ${OBJ_DIR}/obj/lib ${OBJ_DIR}/obj/libexec ${OBJ_DIR}/obj/sys
	@echo
	@echo "***************************************************************"
	@echo "World Build For ${_ARCH} Started On `LC_ALL=C date`"
	@echo "***************************************************************"
	@echo
	@echo "***************************************************************"
	@echo "Step 1a: Build libcxxabi (C++ ABI)"
	@echo "***************************************************************"
	cd ${CURDIR}/contrib/libcxxabi; ${MAKE} all
	@echo
	@echo "***************************************************************"
	@echo "Step 1b: Build libcxx (LLVM libc++)"
	@echo "***************************************************************"
	cd ${CURDIR}/contrib/libcxx; ${MAKE} all
	@echo
	@echo "***************************************************************"
	@echo "Step 1: Build World Libraries"
	@echo "***************************************************************"
	cd ${WORLD_LIB_SRC}; ${WMAKE} all
	@echo
	@echo "***************************************************************"
	@echo "Step 2: Build World Libexec"
	@echo "***************************************************************"
	cd ${WORLD_LIBEXEC_SRC}; ${WMAKE} all
	@echo
	@echo "***************************************************************"
	@echo "Step 3: Build World Binaries"
	@echo "***************************************************************"
	cd ${WORLD_BIN_SRC}; ${WMAKE} all
	@echo
	@echo "***************************************************************"
	@echo "World Build For ${_ARCH} Completed On `LC_ALL=C date`"
	@echo "***************************************************************"

# ── Disk image ───────────────────────────────────────────────────────────────

# Build a fresh bootable FAT32 disk image from scratch (GRUB + kernel + world).
# Always authoritative — use this for releases or a clean initial image.
image:
	@sh tools/mkimage.sh ${DISK_IMAGE}

# ── Mount / unmount ──────────────────────────────────────────────────────────

# Mount the FAT32 partition from the disk image.  Stores the disk device in
# ${_DEV_FILE} so unmount-image can reference it without knowing /dev/diskN.
mount-image:
	@echo "==> Mounting ${DISK_IMAGE} at ${MOUNT_POINT}"
	@hdiutil attach -imagekey diskimage-class=CRawDiskImage ${DISK_IMAGE} | \
	  awk '/FDisk_partition_scheme/{print $$1}' > ${_DEV_FILE}
	@echo "    Device: $$(cat ${_DEV_FILE})"

unmount-image:
	@echo "==> Unmounting ${MOUNT_POINT}"
	@if [ -f ${_DEV_FILE} ]; then \
	  hdiutil detach $$(cat ${_DEV_FILE}) && rm -f ${_DEV_FILE}; \
	else \
	  echo "WARNING: ${_DEV_FILE} not found — image may not be mounted"; \
	fi

# ── Install targets ──────────────────────────────────────────────────────────

# Copy the kernel into the mounted disk image.
install-kernel:
	@echo
	@echo "***************************************************************"
	@echo "Kernel Install For ${_ARCH} Started On `LC_ALL=C date`"
	@echo "***************************************************************"
	@${MAKE} mount-image
	cp ${OBJ_DIR}/boot/kernel ${MOUNT_POINT}/boot/kernel/kernel
	@${MAKE} unmount-image
	@echo "***************************************************************"
	@echo "Kernel Install Completed On `LC_ALL=C date`"
	@echo "***************************************************************"

# Copy world binaries, libraries, headers, and the full source tree into the
# mounted disk image.  The source layout mirrors FreeBSD convention so that
# UbixOS can eventually build itself:
#
#   /bin /lib /libexec   — compiled world
#   /etc                 — system config files
#   /usr/include         — userland headers
#   /usr/src             — full source tree (kernel + world, no build artifacts)
install-world:
	@echo
	@echo "***************************************************************"
	@echo "World Install For ${_ARCH} Started On `LC_ALL=C date`"
	@echo "***************************************************************"
	@${MAKE} mount-image
	@echo
	@echo "--- Step 1: World binaries and libraries"
	find build/bin     -maxdepth 1 -type f -exec cp {} ${MOUNT_POINT}/bin/     \;
	find build/lib     -maxdepth 1 -type f -exec cp {} ${MOUNT_POINT}/lib/     \;
	find build/libexec -maxdepth 1 -type f -exec cp {} ${MOUNT_POINT}/libexec/ \;
	@echo "--- Step 2: System config"
	cp -pr etc/* ${MOUNT_POINT}/etc/
	@echo "--- Step 2b: Assets → /var"
	mkdir -p ${MOUNT_POINT}/var/background
	[ -f sys/sde/assets/ubix.bmp ] && cp sys/sde/assets/ubix.bmp ${MOUNT_POINT}/var/background/ubix.bmp || true
	@echo "--- Step 3: Headers → /usr/include"
	mkdir -p ${MOUNT_POINT}/usr/include
	rsync -r --no-perms --no-owner --no-group \
	  include/ ${MOUNT_POINT}/usr/include/
	@echo "--- Step 4: Source tree → /usr/src"
	mkdir -p ${MOUNT_POINT}/usr/src
	rsync -r --no-perms --no-owner --no-group \
	  --exclude='.git'       \
	  --exclude='build'      \
	  --exclude='ubixos.img' \
	  --exclude='serial.log' \
	  --exclude='*.o'        \
	  --exclude='*.d'        \
	  . ${MOUNT_POINT}/usr/src/
	@${MAKE} unmount-image
	@echo
	@echo "***************************************************************"
	@echo "World Install Completed On `LC_ALL=C date`"
	@echo "***************************************************************"

install: install-world install-kernel

# ── QEMU ─────────────────────────────────────────────────────────────────────

# Boot the disk image in QEMU (primary IDE master, boot from HD).
# Serial output is captured to serial.log for post-mortem inspection.
run:
	qemu-system-i386 -m 256 -drive file=${DISK_IMAGE},format=raw,if=ide,index=0 \
	  -serial file:serial.log -vga std \
	  -device e1000,netdev=net0 -netdev user,id=net0

# Headless run: no display, serial to stdout.  Ctrl-C to stop.
run-debug:
	qemu-system-i386 -m 256 -drive file=${DISK_IMAGE},format=raw,if=ide,index=0 \
	  -nographic -serial stdio \
	  -device e1000,netdev=net0 -netdev user,id=net0

# ── Maintenance ───────────────────────────────────────────────────────────────

# Update just the kernel in an existing disk image without a full rebuild.
kernel-to-image:
	mcopy -o -i ${DISK_IMAGE}@@1M ${OBJ_DIR}/boot/kernel ::/boot/kernel/kernel

clean-kernel:
	(cd sys;${MAKE} clean)

clean:
	(cd sys;${MAKE} clean)
	(cd bin;${WMAKE} clean)
	(cd lib;${WMAKE} clean)
	(cd libexec;${WMAKE} clean)
	(cd contrib/libcxxabi;${MAKE} clean)
	(cd contrib/libcxx;${MAKE} clean)
