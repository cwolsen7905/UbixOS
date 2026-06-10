# The System Makefile (C) 2002-2026 The UbixOS Project
# Do not override MAKE — let bmake set it to itself so recursive invocations
# stay within bmake rather than falling back to GNU make.

CURDIR=${.CURDIR}

# UbixOS manages its own object directories via OBJ_DIR/OBJDIR variables.
# Disable bmake's auto-objdir feature so targets like 'musl' don't get
# redirected to build/obj/musl/ where a GNU Makefile symlink confuses bmake.
MKOBJDIRS=no

include Makefile.incl

OBJ_DIR?= ${CURDIR}/build/${_ARCH}

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

WMAKE=${MAKE} ${WORLD_FLAGS} CROSS_M32="${CROSS_M32}" INCLUDE=${WORLD_INC} BUILD_DIR=${OBJ_DIR}

DISK_IMAGE?=ubixos.img

# USB mass-storage test image (64 MB FAT32, populated by bmake usb-image).
# bmake run attaches it automatically if the file exists.
USB_IMAGE?=usb.img
USB_IMAGE_MB?=512

# Demo MP3 bundled in the USB image for mp3play testing.
# "Investigations" by Kevin MacLeod — CC BY 3.0 (incompetech.com)
DEMO_MP3_URL?=https://incompetech.com/music/royalty-free/mp3-royaltyfree/Investigations.mp3
DEMO_MP3_LOCAL?=/tmp/ubixos_demo.mp3

# Mount point where the FAT32 partition appears after bmake mount-image.
# macOS auto-names it from the FAT volume label; override on the command line
# if needed (e.g. bmake mount-image MOUNT_POINT=/mnt/ubixos).
MOUNT_POINT?=/Volumes/SYS

# Temp file used to pass the hdiutil disk device between mount/unmount.
_DEV_FILE=/tmp/.ubixos_dev

# Optional USB disk flags injected into QEMU run targets when usb.img exists.
# port=2 pins the storage device to root port 2 so QEMU does not insert a
# virtual hub (which we have no driver for).  usb-kbd is pinned to port=1.
_USB_FLAGS!= test -f ${USB_IMAGE} && \
	echo "-drive file=${USB_IMAGE},format=raw,if=none,id=usbdisk -device usb-storage,bus=uhci-bus.0,port=2,drive=usbdisk" || \
	echo ""

# ── Primary targets ──────────────────────────────────────────────────────────

.PHONY: all kernel kernel-i386 kernel-aarch64 musl-libc world makeuser image image-i386 image-aarch64 image-arm usb-image \
        mount-image unmount-image \
        install-kernel install-world install \
        run run-debug run-i386 run-debug-i386 run-aarch64 run-debug-aarch64 \
        run-en0 run-shared \
        kernel-to-image clean-kernel clean

all: kernel world image

# `kernel` is arch-dispatched.  i386 builds the full tree (sys/Makefile); aarch64
# builds the minimal Phase-11 bring-up image (start.S + boot.c) standalone, since
# the generic subsystems aren't ported to aarch64 yet.
kernel: kernel-${_ARCH}

kernel-i386:
	@mkdir -p ${OBJ_DIR}/boot ${OBJ_DIR}/obj/sys
	@cd sys;${MAKE}

# AArch64 bring-up kernel: assemble the entry, compile the PL011 banner, link at
# the QEMU `virt` RAM base.  Standalone — does NOT descend into sys/Makefile.
# Kernel compile flags shared by aarch64 .S and .c — the same -nostdinc +
# sys/include + freestanding conventions as the i386 kernel (so generic kernel
# objects can link in), with the aarch64 ISA flags from KERN_TARGET_CFLAGS.
AARCH64_KCFLAGS = ${KERN_TARGET_CFLAGS} -DDEBUG_SYSCTL -O -Wall -Wno-incompatible-pointer-types \
	-nostdlib -nostdinc -fno-builtin -fno-exceptions -ffreestanding -fno-pie -fno-pic \
	-fno-stack-protector -mno-outline-atomics -I${CURDIR}/sys/include -I${CURDIR}/sys/arch/aarch64

# Arch-neutral kernel sources now linked into the aarch64 kernel.  Grows as more
# of the generic kernel is ported; the scheduler core is the first to link.
AARCH64_GENERIC_SRCS = \
	sys/kern/sched_core.c \
	sys/kern/sched_dispatch.c \
	sys/kern/callout.c \
	sys/kern/random.c \
	sys/kern/vitals.c \
	sys/fs/vfs/vfs.c \
	sys/fs/vfs/bcache.c \
	sys/vmm/vmm_memory.c \
	sys/vmm/vmm_uregion.c \
	sys/lib/kmalloc.c \
	sys/lib/kprintf.c \
	sys/lib/kconsole.c \
	sys/lib/strlen.c \
	sys/vmm/vm_filecache.c \
	sys/kern/elf64_load.c \
	sys/kern/klog.c \
	sys/kern/descrip.c \
	sys/kern/kern_fork.c \
	sys/kern/kern_exec.c \
	sys/kern/access.c \
	sys/fs/vfs/mount.c \
	sys/fs/vfs/file.c \
	sys/fs/procfs/procfs.c \
	sys/fs/devfs/devfs.c \
	sys/fs/ramfs/ramfs.c \
	sys/fs/fat/fat_bpb.c \
	sys/fs/fat/fat_clust.c \
	sys/fs/fat/fat_dir.c \
	sys/fs/fat/fat_file.c \
	sys/fs/fat/fat_sector.c \
	sys/fs/fat/fat_vfs.c \
	sys/fs/vfs/stat.c \
	sys/kern/syscalls.c \
	sys/kern/syscall_dispatch.c \
	sys/posix/syscalls_posix.c \
	sys/posix/gen_calls.c \
	sys/posix/time.c \
	sys/mpi/system.c \
	sys/mpi/message.c \
	sys/mpi/mpi_syscalls.c \
	sys/posix/vfs_calls.c \
	sys/posix/tty.c \
	sys/posix/signal.c \
	sys/posix/pipe.c \
	sys/posix/kern_pipe.c \
	sys/kern/ubthread.c \
	sys/posix/sem.c \
	sys/net/core/def.c \
	sys/net/core/dns.c \
	sys/net/core/inet_chksum.c \
	sys/net/core/init.c \
	sys/net/core/ip.c \
	sys/net/core/mem.c \
	sys/net/core/memp.c \
	sys/net/core/netif.c \
	sys/net/core/pbuf.c \
	sys/net/core/raw.c \
	sys/net/core/stats.c \
	sys/net/core/sys.c \
	sys/net/core/tcp.c \
	sys/net/core/tcp_in.c \
	sys/net/core/tcp_out.c \
	sys/net/core/timeouts.c \
	sys/net/core/udp.c \
	sys/net/core/ipv4/autoip.c \
	sys/net/core/ipv4/dhcp.c \
	sys/net/core/ipv4/etharp.c \
	sys/net/core/ipv4/icmp.c \
	sys/net/core/ipv4/igmp.c \
	sys/net/core/ipv4/ip4.c \
	sys/net/core/ipv4/ip4_addr.c \
	sys/net/core/ipv4/ip4_frag.c \
	sys/net/api/api_lib.c \
	sys/net/api/api_msg.c \
	sys/net/api/err.c \
	sys/net/api/netbuf.c \
	sys/net/api/sockets.c \
	sys/net/api/tcpip.c \
	sys/net/netif/ethernet.c \
	sys/net/net/sys_arch.c

kernel-aarch64:
	@mkdir -p ${OBJ_DIR}/boot ${OBJ_DIR}/obj/sys
	@for f in `find ${CURDIR}/sys/arch/aarch64 -name '*.S'`; do \
	    o=${OBJ_DIR}/obj/sys/`basename $$f .S`.o; \
	    echo "${CROSS_PREFIX}gcc [asm] $$f"; \
	    ${CROSS_PREFIX}gcc ${AARCH64_KCFLAGS} -c $$f -o $$o || exit 1; \
	done
	@for f in `find ${CURDIR}/sys/arch/aarch64 -name '*.c'`; do \
	    o=${OBJ_DIR}/obj/sys/`basename $$f .c`.o; \
	    echo "${CROSS_PREFIX}gcc [c]   $$f"; \
	    ${CROSS_PREFIX}gcc ${AARCH64_KCFLAGS} -std=c99 -c $$f -o $$o || exit 1; \
	done
	@for f in ${AARCH64_GENERIC_SRCS}; do \
	    o=${OBJ_DIR}/obj/sys/`basename $$f .c`.o; \
	    echo "${CROSS_PREFIX}gcc [gen] $$f"; \
	    ${CROSS_PREFIX}gcc ${AARCH64_KCFLAGS} -std=c99 -c ${CURDIR}/$$f -o $$o || exit 1; \
	done
	@echo "${CROSS_PREFIX}gcc [user] tools/aarch64-user/hello.c -> hello.elf (embedded)"
	@${CROSS_PREFIX}gcc -march=armv8-a -mgeneral-regs-only -static -nostdlib -nostartfiles \
	    -ffreestanding -fno-pic -fno-pie -O2 -Wl,-Ttext=0x100000000 -e _start \
	    ${CURDIR}/tools/aarch64-user/hello.c -o ${OBJ_DIR}/hello.elf || exit 1
	@cd ${OBJ_DIR} && ${CROSS_PREFIX}objcopy -I binary -O elf64-littleaarch64 -B aarch64 \
	    hello.elf ${OBJ_DIR}/obj/sys/hello_embed.o || exit 1
	@echo "embedding the musl-linked demo (built only if musl libc exists)"
	@if [ -f ${OBJ_DIR}/lib/libc.a ]; then \
	    ${CROSS_PREFIX}gcc -static -no-pie -fno-pie -ffreestanding -fno-stack-protector -O2 \
	        -nostdinc -isystem ${CURDIR}/contrib/musl/include -isystem ${CURDIR}/contrib/musl/arch/aarch64 \
	        -isystem ${CURDIR}/contrib/musl/arch/generic -isystem ${OBJ_DIR}/obj/musl/obj/include \
	        -nostdlib -Wl,-Ttext-segment=0x100000000 \
	        ${OBJ_DIR}/obj/musl/lib/crt1.o ${OBJ_DIR}/obj/musl/lib/crti.o \
	        ${CURDIR}/tools/aarch64-user/hello_musl.c \
	        ${OBJ_DIR}/lib/libc.a ${LIBGCC} ${OBJ_DIR}/obj/musl/lib/crtn.o \
	        -o ${OBJ_DIR}/hello_musl.elf || exit 1; \
	else \
	    head -c 16 /dev/zero > ${OBJ_DIR}/hello_musl.elf; \
	fi
	@cd ${OBJ_DIR} && ${CROSS_PREFIX}objcopy -I binary -O elf64-littleaarch64 -B aarch64 \
	    hello_musl.elf ${OBJ_DIR}/obj/sys/hello_musl_embed.o || exit 1
	@echo "embedding the static boot triad init/login/sh + spin + mpitest + authd_min (built only if musl libc exists)"
	@for prog in init login sh spin mpitest pipetest faulttest dirtest authd_min; do \
	    if [ -f ${OBJ_DIR}/lib/libc.a ]; then \
	        ${CROSS_PREFIX}gcc -static -no-pie -fno-pie -ffreestanding -fno-stack-protector -O2 \
	            -nostdinc -isystem ${CURDIR}/contrib/musl/include -isystem ${CURDIR}/contrib/musl/arch/aarch64 \
	            -isystem ${CURDIR}/contrib/musl/arch/generic -isystem ${OBJ_DIR}/obj/musl/obj/include \
	            -nostdlib -Wl,-Ttext-segment=0x100000000 \
	            ${OBJ_DIR}/obj/musl/lib/crt1.o ${OBJ_DIR}/obj/musl/lib/crti.o \
	            ${CURDIR}/tools/aarch64-user/$$prog.c \
	            ${OBJ_DIR}/lib/libc.a ${LIBGCC} ${OBJ_DIR}/obj/musl/lib/crtn.o \
	            -o ${OBJ_DIR}/$$prog.elf || exit 1; \
	    else \
	        head -c 16 /dev/zero > ${OBJ_DIR}/$$prog.elf; \
	    fi; \
	    echo "${CROSS_PREFIX}objcopy [embed] $$prog.elf"; \
	    ( cd ${OBJ_DIR} && ${CROSS_PREFIX}objcopy -I binary -O elf64-littleaarch64 -B aarch64 \
	        $$prog.elf ${OBJ_DIR}/obj/sys/$${prog}_embed.o ) || exit 1; \
	done
	@echo "embedding a dynamic (PIE) hello + libc.so (the musl dynamic linker) for the linker test"
	@if [ -f ${OBJ_DIR}/lib/libc.so ]; then \
	    ${CROSS_PREFIX}gcc -fPIC -O2 -ffreestanding -fno-stack-protector \
	        -nostdinc -isystem ${CURDIR}/contrib/musl/include -isystem ${CURDIR}/contrib/musl/arch/aarch64 \
	        -isystem ${CURDIR}/contrib/musl/arch/generic -isystem ${OBJ_DIR}/obj/musl/obj/include \
	        -nostdlib -pie -Wl,-dynamic-linker,/lib/ld-musl-aarch64.so.1 \
	        ${OBJ_DIR}/obj/musl/lib/Scrt1.o ${OBJ_DIR}/obj/musl/lib/crti.o \
	        ${CURDIR}/tools/aarch64-user/hello_musl.c \
	        -L${OBJ_DIR}/lib -lc ${LIBGCC} ${OBJ_DIR}/obj/musl/lib/crtn.o \
	        -o ${OBJ_DIR}/hello_dyn.elf || exit 1; \
	    cp ${OBJ_DIR}/lib/libc.so ${OBJ_DIR}/ld-musl-aarch64.so.1; \
	else \
	    head -c 16 /dev/zero > ${OBJ_DIR}/hello_dyn.elf; \
	    head -c 16 /dev/zero > ${OBJ_DIR}/ld-musl-aarch64.so.1; \
	fi
	@cd ${OBJ_DIR} && ${CROSS_PREFIX}objcopy -I binary -O elf64-littleaarch64 -B aarch64 \
	    hello_dyn.elf ${OBJ_DIR}/obj/sys/hello_dyn_embed.o || exit 1
	@cd ${OBJ_DIR} && ${CROSS_PREFIX}objcopy -I binary -O elf64-littleaarch64 -B aarch64 \
	    ld-musl-aarch64.so.1 ${OBJ_DIR}/obj/sys/ldmusl_embed.o || exit 1
	@echo "embedding a real world binary (busybox cat, PIE) to prove the relinked world runs"
	@if [ -f ${OBJ_DIR}/bin/cat ]; then cp ${OBJ_DIR}/bin/cat ${OBJ_DIR}/worldcat; \
	 else head -c 16 /dev/zero > ${OBJ_DIR}/worldcat; fi
	@cd ${OBJ_DIR} && ${CROSS_PREFIX}objcopy -I binary -O elf64-littleaarch64 -B aarch64 \
	    worldcat ${OBJ_DIR}/obj/sys/worldcat_embed.o || exit 1
	${CROSS_PREFIX}ld -T ${CURDIR}/sys/compile/ldscript.aarch64 -o ${OBJ_DIR}/boot/kernel ${OBJ_DIR}/obj/sys/*.o
	@echo "aarch64 bring-up kernel linked: ${OBJ_DIR}/boot/kernel"

# musl libc per-arch knobs.  i386 uses the FreeBSD stack ABI (-m32, no SSE) and a
# hand-rolled libgcc32; aarch64 uses the stock SVC ABI + the real libgcc.  Both
# build with the FreeBSD syscall numbers (arch/<arch>/bits/syscall.h.in).
# Userland (unlike the kernel) needs FP/SIMD — musl's math code uses double/float,
# which is incompatible with -mgeneral-regs-only.  The kernel enables EL0 FP
# access (CPACR_EL1) so these instructions run at EL0.
.if ${_ARCH} == "aarch64"
MUSL_USER_CFLAGS = -march=armv8-a -ffreestanding -fno-stack-protector
MUSL_LIBCC       = ${LIBGCC}
MUSL_LDEMULATION = aarch64elf
.else
MUSL_USER_CFLAGS = ${CROSS_M32} -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -ffreestanding -fno-stack-protector
MUSL_LIBCC       = ${OBJ_DIR}/lib/libgcc32.a
MUSL_LDEMULATION = elf_i386
.endif

musl-libc:
	@mkdir -p ${OBJ_DIR}/obj/musl ${OBJ_DIR}/lib
	@if [ ! -f ${OBJ_DIR}/obj/musl/config.mak ]; then \
		echo "Configuring musl libc for ${_ARCH}..."; \
		cd ${OBJ_DIR}/obj/musl && ${CURDIR}/contrib/musl/configure \
			--srcdir=${CURDIR}/contrib/musl \
			--target=${_ARCH}-ubixos \
			--enable-shared \
			--enable-static \
			CC="${CROSS_PREFIX}gcc ${MUSL_USER_CFLAGS}" \
			CROSS_COMPILE=${CROSS_PREFIX} \
			LIBCC="" \
			CFLAGS="${MUSL_USER_CFLAGS}"; \
	fi
	@if [ "${_ARCH}" != "aarch64" ]; then \
		${CROSS_PREFIX}gcc ${CROSS_M32} -mno-sse -mno-sse2 -mno-mmx -mno-3dnow \
		    -ffreestanding -fno-pie -fno-pic -nostdinc -std=c99 -O2 \
		    -c ${CURDIR}/tools/libgcc32.c -o ${OBJ_DIR}/obj/musl/libgcc32.o && \
		${CROSS_PREFIX}ar rcs ${OBJ_DIR}/lib/libgcc32.a ${OBJ_DIR}/obj/musl/libgcc32.o; \
	fi
	${GNU_MAKE} -C ${OBJ_DIR}/obj/musl -f ${CURDIR}/contrib/musl/Makefile \
	    srcdir=${CURDIR}/contrib/musl ARCH=${_ARCH} \
	    LD=${CROSS_PREFIX}ld \
	    LDEMULATION=${MUSL_LDEMULATION} \
	    LIBCC=${MUSL_LIBCC}
	cp ${OBJ_DIR}/obj/musl/lib/libc.a ${OBJ_DIR}/lib/musl.a
	cp ${OBJ_DIR}/obj/musl/lib/libc.a ${OBJ_DIR}/lib/libc.a
	cp ${OBJ_DIR}/obj/musl/lib/libc.so ${OBJ_DIR}/lib/libc.so

world:
	@mkdir -p ${OBJ_DIR}/boot ${OBJ_DIR}/bin ${OBJ_DIR}/lib ${OBJ_DIR}/libexec \
	           ${OBJ_DIR}/obj/bin ${OBJ_DIR}/obj/lib ${OBJ_DIR}/obj/libexec ${OBJ_DIR}/obj/sys
	@echo
	@echo "***************************************************************"
	@echo "World Build For ${_ARCH} Started On `LC_ALL=C date`"
	@echo "***************************************************************"
	@echo
	@echo "***************************************************************"
	@echo "Step 0: Build musl libc"
	@echo "***************************************************************"
	${MAKE} musl-libc
	@echo
	@echo "***************************************************************"
	@echo "Step 1a/1b: Build libcxxabi + libcxx (C++)"
	@echo "***************************************************************"
	( cd ${CURDIR}/contrib/libcxxabi; ${MAKE} all ) && \
	( cd ${CURDIR}/contrib/libcxx;    ${MAKE} all )
	@echo
	@echo "***************************************************************"
	@echo "Step 1: Build World Libraries"
	@echo "***************************************************************"
	cd ${WORLD_LIB_SRC}; ${WMAKE} all
	@echo
	@echo "***************************************************************"
	@echo "Step 2: Build World Libexec — i386 only (native ld; musl ld.so on aarch64)"
	@echo "***************************************************************"
	@if [ "${_ARCH}" != "aarch64" ]; then \
	    cd ${WORLD_LIBEXEC_SRC}; ${WMAKE} all; \
	  else echo "skip: native libexec/ld superseded by musl ld.so"; fi
	@echo
	@echo "***************************************************************"
	@echo "Step 3: Build World Binaries"
	@echo "***************************************************************"
	cd ${WORLD_BIN_SRC}; ${WMAKE} all
	@echo
	@echo "***************************************************************"
	@echo "Step 4: Build NetSurf browser (nsfb)"
	@echo "***************************************************************"
	${MAKE} netsurf
	@echo
	@echo "***************************************************************"
	@echo "World Build For ${_ARCH} Completed On `LC_ALL=C date`"
	@echo "***************************************************************"

# Build the NetSurf framebuffer browser (build/bin/nsfb).  Separate target so it
# can be rebuilt on its own; depends on the world libraries (Step 1) being built.
# Driven by a shell script because NetSurf uses its own GNU-make buildsystem.
netsurf:
	SRCTOP=${.CURDIR} BUILD=${OBJ_DIR} _ARCH=${_ARCH} CROSS_PREFIX="${CROSS_PREFIX}" ${.CURDIR}/tools/build-netsurf.sh

# ── Disk image ───────────────────────────────────────────────────────────────

# Root filesystem for the disk image.  Default fat (the only fully-supported
# root today; macOS-mountable).  The native CoW root is the lite-ZFS pool
# (docs/design/ubixfs-pool-plan.md) — its kernel VFS driver is not wired in yet.
# /boot stays FAT regardless so GRUB + the macOS dev loop keep working.
FS ?= fat

# Build a fresh bootable disk image from scratch (GRUB + kernel + world).
# Always authoritative — use this for releases or a clean initial image.
makeuser:
	@echo "==> Compiling and running tools/makeuser.c (PBKDF2-hashes passwords via libpw)"
	cc -O2 -idirafter ${CURDIR}/include \
		-I${CURDIR}/contrib/bearssl/inc -I${CURDIR}/contrib/bearssl/src \
		${CURDIR}/tools/makeuser.c \
		${CURDIR}/lib/libpw/pbkdf2.c ${CURDIR}/lib/libpw/pwhash.c \
		${CURDIR}/contrib/bearssl/src/mac/hmac.c \
		${CURDIR}/contrib/bearssl/src/hash/sha2small.c \
		${CURDIR}/contrib/bearssl/src/codec/dec32be.c \
		${CURDIR}/contrib/bearssl/src/codec/enc32be.c \
		-o ${CURDIR}/tools/makeuser
	cd tools && ./makeuser
	cp tools/userdb etc/userdb
	@echo "==> etc/userdb updated (PBKDF2-hashed)"

# `image` is arch-dispatched so it can never stage one arch's world into the
# other arch's image.  i386 builds the GRUB/FAT ubixos.img; aarch64 builds the
# virtio-blk ubixos-arm.img via image-arm.  Without this, `bmake image
# TARGET=aarch64` (or `bmake TARGET=aarch64`, which runs `all`) would clobber
# the i386 ubixos.img with aarch64 binaries.
image: image-${_ARCH}

image-i386: makeuser
	@echo "==> Disk image root filesystem: FS=${FS}"
	@BUILD=${OBJ_DIR} KERNEL=${OBJ_DIR}/boot/kernel FS=${FS} SRCTOP=${.CURDIR} sh tools/mkimage.sh ${DISK_IMAGE}

# aarch64 disk image: a raw FAT32 image with the dynamically-linked world + the
# musl dynamic linker, mounted at "/" via virtio-blk.  Run after
# `bmake world TARGET=aarch64`; attach with run-aarch64 / run-debug-aarch64.
# image-aarch64 is the arch-dispatch alias; image-arm is the direct entry point.
# PROFILE=base|desktop (default desktop) selects the image profile: a base image
# omits the graphical stack and boots to the text-console login.
PROFILE ?= desktop
image-aarch64: image-arm

image-arm:
	@PROFILE=${PROFILE} sh tools/mkimage-arm.sh ${DISK_IMAGE_ARM} ${OBJ_DIR}

# Build a small FAT32 USB test image with a README.  Attach to QEMU via
# bmake run (auto-detected when usb.img exists) or mount manually with hdiutil.
usb-image:
	@echo "==> Creating USB test image: ${USB_IMAGE} (${USB_IMAGE_MB} MB)"
	@qemu-img create -f raw ${USB_IMAGE} ${USB_IMAGE_MB}M
	@mformat -i ${USB_IMAGE} -F -v UBIX ::
	@printf 'UbixOS USB Test Drive\r\n\r\nDemo track: Investigations by Kevin MacLeod\r\nLicense: CC BY 3.0 -- https://incompetech.com\r\n\r\nTo play: mp3play ubix:/INVESTIGATIONS.MP3\r\n' \
	    > /tmp/ubixos_usb_readme.txt
	@mcopy -i ${USB_IMAGE} /tmp/ubixos_usb_readme.txt ::README.TXT
	@rm -f /tmp/ubixos_usb_readme.txt
	@echo "==> Fetching demo MP3 (Investigations - Kevin MacLeod, CC BY 3.0)..."
	@if [ ! -f ${DEMO_MP3_LOCAL} ]; then \
	    curl -fsSL "${DEMO_MP3_URL}" -o ${DEMO_MP3_LOCAL}; \
	fi
	@mcopy -i ${USB_IMAGE} ${DEMO_MP3_LOCAL} ::INVESTIGATIONS.MP3
	@echo "==> ${USB_IMAGE} ready — run 'bmake run' then: mp3play ubix:/INVESTIGATIONS.MP3"

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

# `run` / `run-debug` are arch dispatchers — they invoke the per-arch recipe for
# the selected ${_ARCH}.  `bmake run` boots i386 today; `bmake run TARGET=aarch64`
# boots the aarch64 kernel in QEMU `virt` once it exists.  The i386 recipes below
# are unchanged (just renamed to run-i386 / run-debug-i386).
run:       run-${_ARCH}
run-debug: run-debug-${_ARCH}

# aarch64 (QEMU `virt`) root disk, attached to run-aarch64 only if it exists.
DISK_IMAGE_ARM?=ubixos-arm.img
_ARM_DISK_FLAGS!= test -f ${DISK_IMAGE_ARM} && \
	echo "-drive file=${DISK_IMAGE_ARM},format=raw,if=none,id=hd0 -device virtio-blk-device,drive=hd0" || \
	echo ""

# ── i386 (PC: IDE disk, e1000, AC97, UHCI keyboard) ─────────────────────────
# Boot the disk image in QEMU (primary IDE master, boot from HD).
# Serial output is captured to serial.log for post-mortem inspection.
# If usb.img exists it is attached as a USB mass-storage device.
run-i386:
	qemu-system-i386 -m 256 -drive file=${DISK_IMAGE},format=raw,if=ide,index=0 \
	  -machine pc \
	  -device piix3-usb-uhci,id=uhci-bus \
	  -device usb-kbd,bus=uhci-bus.0,port=1 \
	  ${_USB_FLAGS} \
	  -serial file:serial.log -vga std \
	  -device e1000,netdev=net0 -netdev user,id=net0 \
	  -object filter-dump,id=f1,netdev=net0,file=/tmp/e1000dump.pcap \
	  -audiodev coreaudio,id=snd0 -device AC97,audiodev=snd0 \
	  -d guest_errors,unimp -D /tmp/qemu_debug.log \
	  --trace "e1000_*"

# Headless i386 run: no display, serial to stdout.  Ctrl-C to stop.
# If usb.img exists it is attached as a USB mass-storage device.
run-debug-i386:
	qemu-system-i386 -m 256 -drive file=${DISK_IMAGE},format=raw,if=ide,index=0 \
	  -machine pc \
	  -device piix3-usb-uhci,id=uhci-bus \
	  -device usb-kbd,bus=uhci-bus.0,port=1 \
	  ${_USB_FLAGS} \
	  -nographic \
	  -device e1000,netdev=net0 -netdev user,id=net0 \
	  -object filter-dump,id=f1,netdev=net0,file=/tmp/e1000dump.pcap

# ── aarch64 (QEMU `virt`: virtio devices, HVF accel on Apple Silicon) ───────
# Boots the kernel directly via -kernel (no GRUB / disk needed for early
# bring-up); virtio-blk is attached only if ${DISK_IMAGE_ARM} exists.  HVF gives
# near-native speed since host and guest are both ARM64.  Serial → serial.log.
run-aarch64:
	qemu-system-aarch64 -machine virt,gic-version=2 -accel hvf -cpu host -m 512 \
	  -kernel ${OBJ_DIR}/boot/kernel \
	  -global virtio-mmio.force-legacy=false \
	  ${_ARM_DISK_FLAGS} \
	  -device virtio-net-device,netdev=net0 -netdev user,id=net0 \
	  -device virtio-gpu-device \
	  -device virtio-keyboard-device -device virtio-mouse-device \
	  -audiodev coreaudio,id=snd0 -device virtio-sound-device,audiodev=snd0 \
	  -serial file:serial.log

# Headless aarch64 run: serial to stdout — the bring-up console.  Ctrl-A X quits.
# force-legacy=false selects the modern (v2) virtio-mmio transport the driver needs.
run-debug-aarch64:
	qemu-system-aarch64 -machine virt,gic-version=2 -accel hvf -cpu host -m 512 \
	  -kernel ${OBJ_DIR}/boot/kernel \
	  -global virtio-mmio.force-legacy=false \
	  ${_ARM_DISK_FLAGS} \
	  -device virtio-net-device,netdev=net0 -netdev user,id=net0 \
	  -nographic

# Headless run with a NE2000 (RTL8029) NIC instead of the e1000, to exercise the
# ne2k driver.  net_init falls back to ne2k when no e1000 is present.  Frames are
# dumped to /tmp/ne2kdump.pcap (decode with: tcpdump -nr /tmp/ne2kdump.pcap).
run-ne2k:
	qemu-system-i386 -m 256 -drive file=${DISK_IMAGE},format=raw,if=ide,index=0 \
	  -machine pc \
	  -device piix3-usb-uhci,id=uhci-bus \
	  -device usb-kbd,bus=uhci-bus.0,port=1 \
	  ${_USB_FLAGS} \
	  -nographic \
	  -device ne2k_pci,netdev=net0 -netdev user,id=net0 \
	  -object filter-dump,id=f1,netdev=net0,file=/tmp/ne2kdump.pcap

# Bridge NIC to en0 (requires sudo on macOS — vmnet-bridged needs entitlements).
# The VM appears on your LAN and gets a real IP from your router's DHCP server.
# Use this to bypass QEMU SLIRP and verify the e1000 driver against a real DHCP.
run-en0:
	sudo qemu-system-i386 -m 256 -drive file=${DISK_IMAGE},format=raw,if=ide,index=0 \
	  -serial file:serial.log -vga std \
	  -device e1000,netdev=net0 -netdev vmnet-bridged,id=net0,ifname=en0 \
	  -object filter-dump,id=f1,netdev=net0,file=/tmp/e1000dump.pcap

# vmnet-shared: macOS NAT + its own DHCP server, also requires sudo.
# Useful when en0 is Wi-Fi and bridged mode is unreliable.
run-shared:
	sudo qemu-system-i386 -m 256 -drive file=${DISK_IMAGE},format=raw,if=ide,index=0 \
	  -serial file:serial.log -vga std \
	  -device e1000,netdev=net0 -netdev vmnet-shared,id=net0 \
	  -object filter-dump,id=f1,netdev=net0,file=/tmp/e1000dump.pcap

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
	@if [ -f ${OBJ_DIR}/obj/musl/Makefile ]; then \
		${GNU_MAKE} -C ${OBJ_DIR}/obj/musl clean; \
	fi
	rm -rf ${OBJ_DIR}/obj/lib
	rm -rf contrib/netsurf/build contrib/netsurf-nsgenbind/build-* \
	    ${OBJ_DIR}/netsurf-pc ${OBJ_DIR}/netsurf-tools ${OBJ_DIR}/netsurf-build.log \
	    ${OBJ_DIR}/nsgenbind-build.log ${OBJ_DIR}/bin/nsfb
