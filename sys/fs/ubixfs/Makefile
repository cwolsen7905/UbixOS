# (C) 2002-2026 The UbixOS Project

include ../../../Makefile.incl
include ../../Makefile.incl

# UbixFS pooled copy-on-write (lite-ZFS) filesystem driver.  Replaces the dead
# v1 BeFS driver (thread/ubixfs/directory/block/dir_cache.c — still in-tree but
# no longer built).  The portable core lives in lib/ubixfs_core and is compiled
# here with the freestanding compat shims + the core/shared include paths, the
# i386 mirror of the aarch64 Makefile's UBIXFS_KERN_SRCS loop.
OBJS = ubfs_kshim.o ubfs_vfs.o fletcher.o ubfs_spa.o ubfs_dmu.o ubfs_dsl.o ubfs_dir.o ubfs_fs.o

UBFS_CORE = ${.CURDIR}/../../../lib/ubixfs_core
.PATH: ${UBFS_CORE}
INCLUDES += -I${.CURDIR}/compat -I${UBFS_CORE} -I${.CURDIR}/../../../include

.include "${UBIX_MK}/ubix.kern.mk"
