# The System Makefile (C) 2002-2026 The UbixOS Project
# Do not override MAKE — let bmake set it to itself so recursive invocations
# stay within bmake rather than falling back to GNU make.

CURDIR=${.CURDIR}

# UbixOS manages its own object directories via OBJ_DIR/OBJDIR variables.
# Disable bmake's auto-objdir feature so targets like 'musl' don't get
# redirected to build/obj/musl/ where a GNU Makefile symlink confuses bmake.
MKOBJDIRS=no

include Makefile.incl

# OBJ_DIR is set by Makefile.incl (${SRCTOP}/build/${_ARCH} by default) and is
# overridable on the command line, e.g. OBJ_DIR=/usr/obj/${_ARCH} for an
# on-device /usr/src -> /usr/obj build.  No redefinition here — a second `?=`
# would just shadow the shared default with the same value.

CLEANDIR=clean

WORLD_LIB_SRC=${CURDIR}/lib
WORLD_LIBEXEC_SRC=${CURDIR}/libexec
WORLD_BIN_SRC=${CURDIR}/bin
# Additional program trees (FreeBSD-style hierarchy): sbin -> /sbin,
# usr.bin -> /usr/bin, usr.sbin -> /usr/sbin, tests -> /usr/tests.  Each program
# tree sets BINDIR in its Makefile.incl so its binaries land under the matching
# ${OBJ_DIR}/<dir>; mkimage stages them into the image at the same path.
WORLD_SBIN_SRC=${CURDIR}/sbin
WORLD_USRBIN_SRC=${CURDIR}/usr.bin
WORLD_USRSBIN_SRC=${CURDIR}/usr.sbin
WORLD_TESTS_SRC=${CURDIR}/tests
WORLD_INC="-I${CURDIR}/include -I${CURDIR}/lib/objgfx40/ -I${CURDIR}/contrib/libcxxabi/include"
# Toolchain (CC/CXX/AS/AR/LD/NM/OBJCOPY/RANLIB) is selected centrally by
# ubix.toolchain.mk and reaches the world sub-makes via the exported TOOLCHAIN:
# each sub-make re-includes ubix.toolchain.mk and recomputes the tools.  We do NOT
# pass them here, because the clang profile's CC is multi-word (clang --target …
# --ld-path …) and bmake would mis-split it as a command-line variable.
WORLD_FLAGS=_ARCH=${_ARCH} OBJDUMP=

WMAKE=${MAKE} ${WORLD_FLAGS} CROSS_M32="${CROSS_M32}" INCLUDE=${WORLD_INC} BUILD_DIR=${OBJ_DIR}

DISK_IMAGE?=ubixos.img
DISK_IMAGE_X86_64?=ubixos-x86_64.img

# Host interface to bridge the guest onto for SSH access.  vmnet-bridged
# requires QEMU to run as root (or with the com.apple.vm.networking entitlement).
# Override on the command line: bmake run-aarch64 BRIDGE_IF=en1
BRIDGE_IF?=en0

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

.PHONY: all kernel kernel-aarch64 kernel-x86_64 run-x86_64 run-debug-x86_64 musl-libc world makeuser image image-aarch64 image-arm usb-image \
        mount-image unmount-image \
        install-kernel install-world install \
        run-aarch64 run-debug-aarch64 run-claude \
        kernel-to-image clean-kernel clean

# `all` is arch-aware: every MMU-class arch builds the full system (kernel + world
# + image).  x86_64 now boots its musl world to the graphical desktop (views over
# the std-VGA framebuffer), same as i386/aarch64.
# world BEFORE kernel: the kernel embeds musl-linked demo programs (init/login/sh,
# the linker test, worldcat) into its image, so it needs world's musl libc + bin/
# already built.  Building kernel first fails after a clean (no bits/alltypes.h).
all: world kernel image

# `kernel` is arch-dispatched.  master is 64-bit only: aarch64 (default) + x86_64,
# each built standalone from the top Makefile (explicit GENERIC_SRCS lists + a
# per-arch `find sys/arch/<arch>`).  i386 is frozen on releng/2 — not built here.
kernel: kernel-${_ARCH}

# AArch64 bring-up kernel: assemble the entry, compile the PL011 banner, link at
# the QEMU `virt` RAM base.  Standalone — does NOT descend into sys/Makefile.
# Curated "this is almost always a real bug" warning classes promoted to errors
# on first-party code (kernel + our userland).  These catch the LP64 / ABI bug
# families uBixOS has actually been bitten by (missing prototype -> int return
# truncates a 64-bit pointer; int<->pointer casts; fall-off-the-end returns).
# Deliberately NARROW: we do NOT -Werror the noisy style classes, and we keep
# -Wno-incompatible-pointer-types / int-conversion out of the set (the kernel
# still suppresses the former).  musl itself builds with this same family.
WERROR_BUGCLASSES = -Werror=implicit-function-declaration -Werror=implicit-int \
	-Werror=int-to-pointer-cast -Werror=pointer-to-int-cast -Werror=return-type

# Kernel compile flags shared by aarch64 .S and .c — the same -nostdinc +
# sys/include + freestanding conventions as the i386 kernel (so generic kernel
# objects can link in), with the aarch64 ISA flags from KERN_TARGET_CFLAGS.
AARCH64_KCFLAGS = ${KERN_TARGET_CFLAGS} -DDEBUG_SYSCTL -O -Wall -Wno-incompatible-pointer-types ${WERROR_BUGCLASSES} \
	-nostdlib ${TC_NOSTDINC} -fno-builtin -fno-exceptions -ffreestanding -fno-pie -fno-pic \
	-fno-strict-aliasing \
	-fno-stack-protector -mno-outline-atomics -I${CURDIR}/sys/include -I${CURDIR}/sys/arch/aarch64

# Board build hooks (M1): the Pi kernel build (tools/build-rpi3-kernel.sh) passes
# AARCH64_KCFLAGS_EXTRA=-DBOARD_RPI3 (compile) + AARCH64_LD_EXTRA="--defsym
# KERNEL_PHYS=0x80000" (link) on the command line.  Empty by default, so the QEMU
# virt build is unchanged.
AARCH64_KCFLAGS_EXTRA ?=
AARCH64_LD_EXTRA ?=
AARCH64_KCFLAGS += ${AARCH64_KCFLAGS_EXTRA}

# Arch-neutral kernel sources now linked into the aarch64 kernel.  Grows as more
# of the generic kernel is ported; the scheduler core is the first to link.
AARCH64_GENERIC_SRCS = \
	sys/kern/sched_core.c \
	sys/kern/sched_dispatch.c \
	sys/kern/cpu_enum.c \
	sys/kern/callout.c \
	sys/kern/random.c \
	sys/kern/vitals.c \
	sys/fs/vfs/vfs.c \
	sys/fs/vfs/bcache.c \
	sys/dev/partition.c \
	sys/kern/disk_query.c \
	sys/vmm/vmm_memory.c \
	sys/vmm/vmm_uregion.c \
	sys/lib/kmalloc.c \
	sys/lib/kprintf.c \
	sys/lib/kconsole.c \
	sys/lib/strlen.c \
	sys/lib/rbtree.c \
	sys/vmm/vm_filecache.c \
	sys/vmm/vm_map.c \
	sys/vmm/vmm_demand.c \
	sys/kern/elf64_load.c \
	sys/kern/elf64_demand.c \
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

# UbixFS pooled-CoW filesystem: the portable lite-ZFS core (lib/ubixfs_core,
# unmodified) + the kernel glue.  Compiled with extra include paths so the
# hosted-style core resolves <stdint.h>/<stdlib.h> from the freestanding compat
# shims, its own headers from lib/ubixfs_core, and the on-disk format from the
# shared include/ tree.  See docs/design/ubixfs-pool-plan.md (step K1).
UBIXFS_KERN_SRCS = \
	lib/ubixfs_core/fletcher.c \
	lib/ubixfs_core/ubfs_spa.c \
	lib/ubixfs_core/ubfs_dmu.c \
	lib/ubixfs_core/ubfs_dsl.c \
	lib/ubixfs_core/ubfs_dir.c \
	lib/ubixfs_core/ubfs_fs.c \
	sys/fs/ubixfs/ubfs_kshim.c \
	sys/fs/ubixfs/ubfs_vfs.c \
	sys/fs/ubixfs/ubixfs_selftest.c
UBIXFS_KERN_INCS = -I${CURDIR}/sys/fs/ubixfs/compat -I${CURDIR}/lib/ubixfs_core -I${CURDIR}/include

kernel-aarch64:
	@mkdir -p ${OBJ_DIR}/boot ${OBJ_DIR}/obj/sys
	@# Exclude board/ — per-board bare-metal targets (e.g. opizero2w) carry their
	@# own start.S + linker script (m0.ld, _image_size) and must NOT be swept into
	@# the QEMU virt kernel, where their start.o collides with kern/start.o on the
	@# flat obj/sys/ basename and the wrong one wins by find(1) order.
	@for f in `find ${CURDIR}/sys/arch/aarch64 -name '*.S' -not -path '*/board/*'`; do \
	    o=${OBJ_DIR}/obj/sys/`basename $$f .S`.o; \
	    sh ${CURDIR}/tools/kbuild-cc.sh "${KERN_CC} [asm] " $$o $$f ${KERN_CC} ${KERN_CCFLAGS} ${AARCH64_KCFLAGS} || exit 1; \
	done
	@for f in `find ${CURDIR}/sys/arch/aarch64 -name '*.c' -not -path '*/board/*'`; do \
	    o=${OBJ_DIR}/obj/sys/`basename $$f .c`.o; \
	    sh ${CURDIR}/tools/kbuild-cc.sh "${KERN_CC} [c]   " $$o $$f ${KERN_CC} ${KERN_CCFLAGS} ${AARCH64_KCFLAGS} -std=c99 || exit 1; \
	done
	@for f in ${AARCH64_GENERIC_SRCS}; do \
	    o=${OBJ_DIR}/obj/sys/`basename $$f .c`.o; \
	    sh ${CURDIR}/tools/kbuild-cc.sh "${KERN_CC} [gen] " $$o ${CURDIR}/$$f ${KERN_CC} ${KERN_CCFLAGS} ${AARCH64_KCFLAGS} -std=c99 || exit 1; \
	done
	@for f in ${UBIXFS_KERN_SRCS}; do \
	    o=${OBJ_DIR}/obj/sys/`basename $$f .c`.o; \
	    sh ${CURDIR}/tools/kbuild-cc.sh "${KERN_CC} [ubfs]" $$o ${CURDIR}/$$f ${KERN_CC} ${KERN_CCFLAGS} ${AARCH64_KCFLAGS} ${UBIXFS_KERN_INCS} -std=c99 || exit 1; \
	done
	@echo "${CROSS_PREFIX}gcc [user] tools/aarch64-user/hello.c -> hello.elf (embedded)"
	@${CROSS_PREFIX}gcc -march=armv8-a -mgeneral-regs-only -static -nostdlib -nostartfiles \
	    -ffreestanding -fno-pic -fno-pie -O2 -Wl,-Ttext=0x100000000 -e _start \
	    ${CURDIR}/tools/aarch64-user/hello.c -o ${OBJ_DIR}/hello.elf || exit 1
	@cd ${OBJ_DIR} && ${OBJCOPY} -I binary -O elf64-littleaarch64 -B aarch64 \
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
	@cd ${OBJ_DIR} && ${OBJCOPY} -I binary -O elf64-littleaarch64 -B aarch64 \
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
	    ( cd ${OBJ_DIR} && ${OBJCOPY} -I binary -O elf64-littleaarch64 -B aarch64 \
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
	@cd ${OBJ_DIR} && ${OBJCOPY} -I binary -O elf64-littleaarch64 -B aarch64 \
	    hello_dyn.elf ${OBJ_DIR}/obj/sys/hello_dyn_embed.o || exit 1
	@cd ${OBJ_DIR} && ${OBJCOPY} -I binary -O elf64-littleaarch64 -B aarch64 \
	    ld-musl-aarch64.so.1 ${OBJ_DIR}/obj/sys/ldmusl_embed.o || exit 1
	@echo "embedding a real world binary (busybox cat, PIE) to prove the relinked world runs"
	@if [ -f ${OBJ_DIR}/bin/cat ]; then cp ${OBJ_DIR}/bin/cat ${OBJ_DIR}/worldcat; \
	 else head -c 16 /dev/zero > ${OBJ_DIR}/worldcat; fi
	@cd ${OBJ_DIR} && ${OBJCOPY} -I binary -O elf64-littleaarch64 -B aarch64 \
	    worldcat ${OBJ_DIR}/obj/sys/worldcat_embed.o || exit 1
	${LD} ${AARCH64_LD_EXTRA} -T ${CURDIR}/sys/compile/ldscript.aarch64 -o ${OBJ_DIR}/boot/kernel ${OBJ_DIR}/obj/sys/*.o
	@echo "aarch64 bring-up kernel linked: ${OBJ_DIR}/boot/kernel"

# x86-64 bring-up kernel: assemble the long-mode entry, compile the COM1 banner,
# link low at 1 MB.  Standalone (does NOT descend into sys/Makefile) — the same
# minimal-first approach the aarch64 bring-up used; the generic subsystems + the
# widened i386 drivers link in as the port grows.
X86_64_KCFLAGS = ${KERN_TARGET_CFLAGS} -O -Wall -Wno-incompatible-pointer-types ${WERROR_BUGCLASSES} -nostdlib -nostdinc \
	-fno-builtin -fno-exceptions -ffreestanding -fno-pie -fno-pic -fno-stack-protector \
	-fno-strict-aliasing \
	-I${CURDIR}/sys/include -I${CURDIR}/sys/arch/x86_64

# Arch-neutral kernel sources linked into the x86_64 kernel.  Grows as the port
# advances (mirrors AARCH64_GENERIC_SRCS); Phase 3 links the physical allocator
# + kernel heap.
X86_64_GENERIC_SRCS = \
	sys/vmm/vmm_memory.c \
	sys/lib/kmalloc.c \
	sys/kern/sched_core.c \
	sys/kern/sched_dispatch.c \
	sys/kern/elf64_load.c \
	sys/kern/elf64_demand.c \
	sys/kern/kern_fork.c \
	sys/kern/kern_exec.c \
	sys/kern/syscalls.c \
	sys/kern/syscall_dispatch.c \
	sys/kern/descrip.c \
	sys/kern/vitals.c \
	sys/kern/cpu_enum.c \
	sys/kern/callout.c \
	sys/kern/random.c \
	sys/kern/klog.c \
	sys/kern/endtask.c \
	sys/kern/access.c \
	sys/kern/disk_query.c \
	sys/kern/ubthread.c \
	sys/posix/syscalls_posix.c \
	sys/posix/vfs_calls.c \
	sys/posix/gen_calls.c \
	sys/posix/time.c \
	sys/posix/signal.c \
	sys/posix/pipe.c \
	sys/posix/kern_pipe.c \
	sys/posix/sem.c \
	sys/posix/tty.c \
	sys/mpi/message.c \
	sys/mpi/system.c \
	sys/mpi/mpi_syscalls.c \
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
	sys/net/net/sys_arch.c \
	sys/vmm/vmm_uregion.c \
	sys/vmm/vm_filecache.c \
	sys/vmm/vm_map.c \
	sys/vmm/vmm_demand.c \
	sys/lib/rbtree.c \
	sys/fs/vfs/stat.c \
	sys/fs/procfs/procfs.c \
	sys/fs/devfs/devfs.c \
	sys/fs/ramfs/ramfs.c \
	sys/lib/kprintf.c \
	sys/lib/kconsole.c \
	sys/lib/string.c \
	sys/lib/strlen.c \
	sys/lib/strstr.c \
	sys/dev/partition.c \
	sys/fs/vfs/vfs.c \
	sys/fs/vfs/bcache.c \
	sys/fs/vfs/mount.c \
	sys/fs/vfs/file.c \
	sys/fs/fat/fat_bpb.c \
	sys/fs/fat/fat_clust.c \
	sys/fs/fat/fat_dir.c \
	sys/fs/fat/fat_file.c \
	sys/fs/fat/fat_sector.c \
	sys/fs/fat/fat_vfs.c

kernel-x86_64:
	@mkdir -p ${OBJ_DIR}/boot ${OBJ_DIR}/obj/sys
	@for f in `find ${CURDIR}/sys/arch/x86_64 -name '*.S'`; do \
	    o=${OBJ_DIR}/obj/sys/`basename $$f .S`.o; \
	    sh ${CURDIR}/tools/kbuild-cc.sh "${CROSS_PREFIX}gcc [asm] " $$o $$f ${CROSS_PREFIX}gcc ${X86_64_KCFLAGS} || exit 1; \
	done
	@for f in `find ${CURDIR}/sys/arch/x86_64 -name '*.c'`; do \
	    o=${OBJ_DIR}/obj/sys/`basename $$f .c`.o; \
	    sh ${CURDIR}/tools/kbuild-cc.sh "${CROSS_PREFIX}gcc [c]   " $$o $$f ${CROSS_PREFIX}gcc ${X86_64_KCFLAGS} -std=c99 || exit 1; \
	done
	@for f in ${X86_64_GENERIC_SRCS}; do \
	    o=${OBJ_DIR}/obj/sys/`basename $$f .c`.o; \
	    sh ${CURDIR}/tools/kbuild-cc.sh "${CROSS_PREFIX}gcc [gen] " $$o ${CURDIR}/$$f ${CROSS_PREFIX}gcc ${X86_64_KCFLAGS} -std=c99 || exit 1; \
	done
	@for f in ${UBIXFS_KERN_SRCS}; do \
	    o=${OBJ_DIR}/obj/sys/`basename $$f .c`.o; \
	    sh ${CURDIR}/tools/kbuild-cc.sh "${CROSS_PREFIX}gcc [ubfs]" $$o ${CURDIR}/$$f ${CROSS_PREFIX}gcc ${X86_64_KCFLAGS} ${UBIXFS_KERN_INCS} -std=c99 || exit 1; \
	done
	${CROSS_PREFIX}ld -T ${CURDIR}/sys/compile/ldscript.x86_64 -o ${OBJ_DIR}/boot/kernel ${OBJ_DIR}/obj/sys/*.o
	@echo "x86_64 bring-up kernel linked: ${OBJ_DIR}/boot/kernel"

# Graphical x86_64 run: a std-VGA window (the desktop framebuffer views draws
# into) + serial multiplexed onto stdio (the monitor too: Ctrl-A C toggles).
# Attaches ${DISK_IMAGE_X86_64} (the x86_64 world image from `bmake image
# TARGET=x86_64`) as a legacy virtio-blk-pci disk (disable-modern=true -> the
# I/O-BAR register window the bring-up driver speaks).  On Apple Silicon x86_64
# runs under TCG (no HVF for a foreign arch), so it is emulated — slower but works.
run-x86_64:
	sudo qemu-system-x86_64 -m 256 -smp ${SMP} -cpu qemu64,+x2apic -kernel ${OBJ_DIR}/boot/kernel \
	  -vga std -serial mon:stdio \
	  -drive file=${DISK_IMAGE_X86_64},format=raw,if=none,id=hd0 \
	  -device virtio-blk-pci,drive=hd0,disable-modern=true \
	  -netdev vmnet-bridged,id=n0,ifname=${BRIDGE_IF} \
	  -device virtio-net-pci,netdev=n0,disable-modern=true \
	  -audiodev coreaudio,id=snd0 -device AC97,audiodev=snd0

# Headless x86_64 run: serial only to stdout, no display (the bring-up console for
# debugging).  Ctrl-A X quits.
run-debug-x86_64:
	qemu-system-x86_64 -m 256 -smp ${SMP} -cpu qemu64,+x2apic -kernel ${OBJ_DIR}/boot/kernel -nographic \
	  -drive file=${DISK_IMAGE_X86_64},format=raw,if=none,id=hd0 \
	  -device virtio-blk-pci,drive=hd0,disable-modern=true \
	  -netdev vmnet-bridged,id=n0,ifname=${BRIDGE_IF} \
	  -device virtio-net-pci,netdev=n0,disable-modern=true \
	  -audiodev coreaudio,id=snd0 -device AC97,audiodev=snd0

# musl libc per-arch knobs (64-bit only on master).  aarch64 uses the stock SVC
# ABI; x86_64 the SysV amd64 ABI.  Both build with the FreeBSD syscall numbers
# (arch/<arch>/bits/syscall.h.in) + the real libgcc.  Userland (unlike the kernel)
# needs FP/SIMD — musl's math uses double/float (the kernel enables EL0 FP access
# on aarch64 so these run at EL0).  (i386's -m32/libgcc32 knobs live on releng/2.)
.if ${_ARCH} == "aarch64"
MUSL_USER_CFLAGS = -march=armv8-a -ffreestanding -fno-stack-protector
MUSL_LIBCC       = ${LIBGCC}
MUSL_LDEMULATION = aarch64elf
.elif ${_ARCH} == "x86_64"
# Userland keeps SSE (the SysV amd64 ABI returns floats in XMM + musl's code uses
# it) — unlike the kernel's -mno-sse.  Real libgcc, no -m32 shim.
MUSL_USER_CFLAGS = -ffreestanding -fno-stack-protector
MUSL_LIBCC       = ${LIBGCC}
MUSL_LDEMULATION = elf_x86_64
.else
.error Unsupported TARGET '${_ARCH}' on master — only aarch64 + x86_64 (i386 is on releng/2).
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
	@if [ "${_ARCH}" != "aarch64" ] && [ "${_ARCH}" != "x86_64" ]; then \
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
	           ${OBJ_DIR}/sbin ${OBJ_DIR}/usr/bin ${OBJ_DIR}/usr/sbin ${OBJ_DIR}/usr/tests \
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
	@echo "Step 2: Build World Libexec — i386 only (native ld; musl ld.so on 64-bit)"
	@echo "***************************************************************"
	@if [ "${_ARCH}" != "aarch64" ] && [ "${_ARCH}" != "x86_64" ]; then \
	    cd ${WORLD_LIBEXEC_SRC}; ${WMAKE} all; \
	  else echo "skip: native libexec/ld superseded by musl ld.so"; fi
	@echo
	@echo "***************************************************************"
	@echo "Step 3: Build World Binaries (/bin)"
	@echo "***************************************************************"
	cd ${WORLD_BIN_SRC}; ${WMAKE} all
	@echo
	@echo "***************************************************************"
	@echo "Step 3b: Build /sbin, /usr/bin, /usr/sbin, /usr/tests trees"
	@echo "***************************************************************"
	cd ${WORLD_SBIN_SRC};    ${WMAKE} all
	cd ${WORLD_USRBIN_SRC};  ${WMAKE} all
	cd ${WORLD_USRSBIN_SRC}; ${WMAKE} all
	cd ${WORLD_TESTS_SRC};   ${WMAKE} all
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
# other arch's image: aarch64 -> ubixos-arm.img, x86_64 -> ubixos-x86_64.img,
# both via tools/mkimage.sh.
image: image-${_ARCH}

# aarch64 disk image: a raw FAT32 image with the dynamically-linked world + the
# musl dynamic linker, mounted at "/" via virtio-blk.  Run after
# `bmake world TARGET=aarch64`; attach with run-aarch64 / run-debug-aarch64.
# image-aarch64 is the arch-dispatch alias; image-arm is the direct entry point.
# PROFILE=base|desktop (default desktop) selects the image profile: a base image
# omits the graphical stack and boots to the text-console login.
PROFILE ?= desktop
image-aarch64: image-arm

image-arm:
	@PROFILE=${PROFILE} sh tools/mkimage.sh ${DISK_IMAGE_ARM} ${OBJ_DIR}

# Stage the source tree into the built image's UbixFS pool as a FreeBSD-style
# /usr/src, plus /usr/obj/${_ARCH} (build output) + /usr/share/mk, so `bmake`
# can build the system from /usr/src on-device.  Run AFTER `bmake image`.  Kept
# separate from mkimage.sh (see docs/design/usr-src-build-layout-plan.md).
stage-src: stage-src-${_ARCH}
stage-src-aarch64:
	@sh tools/stage-src.sh ${DISK_IMAGE_ARM} aarch64
stage-src-x86_64:
	@sh tools/stage-src.sh ${DISK_IMAGE_X86_64} x86_64

# x86_64 disk image: the SAME raw FAT32 + musl-ld.so layout as aarch64 (mkimage.sh
# is arch-parameterised via ARCH=), mounted at "/" via virtio-blk-pci.  Run after
# `bmake world TARGET=x86_64`; boot with run-x86_64.
image-x86_64:
	@PROFILE=${PROFILE} ARCH=x86_64 sh tools/mkimage.sh ${DISK_IMAGE_X86_64} ${OBJ_DIR}

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
	cp ${OBJ_DIR}/boot/kernel ${MOUNT_POINT}/kernel/kernel
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
	find ${OBJ_DIR}/bin     -maxdepth 1 -type f -exec cp {} ${MOUNT_POINT}/bin/     \;
	find ${OBJ_DIR}/lib     -maxdepth 1 -type f -exec cp {} ${MOUNT_POINT}/lib/     \;
	find ${OBJ_DIR}/libexec -maxdepth 1 -type f -exec cp {} ${MOUNT_POINT}/libexec/ \;
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
# the selected ${_ARCH}.  `bmake run` boots aarch64 (the default); `bmake run
# TARGET=x86_64` boots the x86_64 kernel.  (i386 run recipes live on releng/2.)
# vCPU count for every run target.  Minimum 2 so SMP / CPU-enumeration is always
# exercised on both arches; override with `bmake run SMP=4`.
SMP ?= 2

run:       run-${_ARCH}
run-debug: run-debug-${_ARCH}

# aarch64 (QEMU `virt`) root disk, attached to run-aarch64 only if it exists.
DISK_IMAGE_ARM?=ubixos-arm.img
_ARM_DISK_FLAGS!= test -f ${DISK_IMAGE_ARM} && \
	echo "-drive file=${DISK_IMAGE_ARM},format=raw,if=none,id=hd0 -device virtio-blk-device,drive=hd0" || \
	echo ""

# ── aarch64 (QEMU `virt`: virtio devices, HVF accel on Apple Silicon) ───────
# Boots the kernel directly via -kernel (no GRUB / disk needed for early
# bring-up); virtio-blk is attached only if ${DISK_IMAGE_ARM} exists.  HVF gives
# near-native speed since host and guest are both ARM64.  Serial → serial.log.
run-aarch64:
	sudo qemu-system-aarch64 -machine virt,gic-version=2 -accel hvf -cpu host -m 512 -smp ${SMP} \
	  -kernel ${OBJ_DIR}/boot/kernel \
	  -global virtio-mmio.force-legacy=false \
	  ${_ARM_DISK_FLAGS} \
	  -device virtio-net-device,netdev=net0 -netdev vmnet-bridged,id=net0,ifname=${BRIDGE_IF} \
	  -device virtio-gpu-device \
	  -device virtio-keyboard-device -device virtio-mouse-device \
	  -audiodev coreaudio,id=snd0 -device virtio-sound-device,audiodev=snd0 \
	  -serial file:serial.log

# Headless aarch64 run: serial to stdout — the bring-up console.  Ctrl-A X quits.
# force-legacy=false selects the modern (v2) virtio-mmio transport the driver needs.
# Uses user-mode (NAT) networking so it needs no sudo / vmnet entitlement — the
# guest DHCPs a 10.0.2.x address from QEMU's built-in stack.  (run-aarch64, the
# human graphical run, keeps bridged networking.)
run-debug-aarch64:
	qemu-system-aarch64 -machine virt,gic-version=2 -accel hvf -cpu host -m 512 -smp ${SMP} \
	  -kernel ${OBJ_DIR}/boot/kernel \
	  -global virtio-mmio.force-legacy=false \
	  ${_ARM_DISK_FLAGS} \
	  -device virtio-net-device,netdev=net0 -netdev user,id=net0 \
	  -nographic

# TCG (software-emulated CPU) aarch64 run — for debugging the HVF "isv" abort.
# HVF aborts opaquely when the guest makes an MMIO access it can't decode (a wild
# pointer landing in the 0x0A000000 virtio window).  Under TCG that same access is
# handled by the GUEST's own fault path, so the kernel logs the faulting process +
# PC + FAR — revealing the wild-pointer source.  Slow (no HW accel); -cpu max (not
# "host", which means HVF passthrough).  Full device set + serial on stdio so you
# can drive it and watch the fault.  `bmake run-tcg-aarch64 AUDIO=0` drops sound.
AUDIO?=1
.if ${AUDIO} == "0"
_ARM_SOUND=
.else
_ARM_SOUND=-audiodev coreaudio,id=snd0 -device virtio-sound-device,audiodev=snd0
.endif
run-tcg-aarch64:
	sudo qemu-system-aarch64 -machine virt,gic-version=2 -accel tcg -cpu max -m 512 -smp ${SMP} \
	  -kernel ${OBJ_DIR}/boot/kernel \
	  -global virtio-mmio.force-legacy=false \
	  ${_ARM_DISK_FLAGS} \
	  -device virtio-net-device,netdev=net0 -netdev vmnet-bridged,id=net0,ifname=${BRIDGE_IF} \
	  -device virtio-gpu-device \
	  -device virtio-keyboard-device -device virtio-mouse-device \
	  ${_ARM_SOUND} \
	  -serial file:serial.log

# Agent (Claude) run: the graphical desktop opens in a window for the human, AND
# the serial console is exposed on a unix socket so the coding agent can type
# commands + read text output directly — no driving the GUI over VNC.  Serial is
# tee'd to a logfile so the full boot is captured even before the agent connects.
# Uses user-mode (NAT) networking so the agent can launch it with no sudo / vmnet
# entitlement (the guest DHCPs a 10.0.2.x address).  run-aarch64 keeps bridged.
#   serial socket : /tmp/ubixos-claude.sock        (interactive — `nc -U` or pipe)
#   serial log    : /tmp/ubixos-claude-serial.log
#   qemu monitor  : /tmp/ubixos-claude-mon.sock     (screenshot/sendkey fallback)
run-claude:
	@echo "==> UbixOS desktop opens in a window; agent serial on /tmp/ubixos-claude.sock"
	qemu-system-aarch64 -machine virt,gic-version=2 -accel hvf -cpu host -m 512 -smp ${SMP} \
	  -kernel ${OBJ_DIR}/boot/kernel \
	  -global virtio-mmio.force-legacy=false \
	  ${_ARM_DISK_FLAGS} \
	  -device virtio-net-device,netdev=net0 -netdev user,id=net0 \
	  -device virtio-gpu-device \
	  -device virtio-keyboard-device -device virtio-mouse-device \
	  -audiodev coreaudio,id=snd0 -device virtio-sound-device,audiodev=snd0 \
	  -chardev socket,id=claudeser,path=/tmp/ubixos-claude.sock,server=on,wait=off,logfile=/tmp/ubixos-claude-serial.log \
	  -serial chardev:claudeser \
	  -monitor unix:/tmp/ubixos-claude-mon.sock,server,nowait

# ── Maintenance ───────────────────────────────────────────────────────────────

# Update just the kernel in an existing disk image without a full rebuild.
kernel-to-image:
	mcopy -o -i ${DISK_IMAGE}@@1M ${OBJ_DIR}/boot/kernel ::/kernel/kernel

clean-kernel:
	rm -rf ${OBJ_DIR}/obj/sys ${OBJ_DIR}/boot

clean:
	rm -rf ${OBJ_DIR}/obj/sys ${OBJ_DIR}/boot
	(cd bin;${WMAKE} clean)
	(cd sbin;${WMAKE} clean)
	(cd usr.bin;${WMAKE} clean)
	(cd usr.sbin;${WMAKE} clean)
	(cd tests;${WMAKE} clean)
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
