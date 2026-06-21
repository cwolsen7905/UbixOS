/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * ubfs_vdev_image — a host (macOS/FreeBSD/Linux) file-backed vdev for the
 * portable UbixFS core (lib/ubixfs_core).  Opens an image file and presents it
 * to the core as a single vdev, transparently handling two image shapes:
 *
 *   1. a *raw pool* image (the pool label sits at byte 0), or
 *   2. a *full disk image* with an MBR partition table — the vdev is offset to
 *      the UbixFS pool partition (type 0x9C, MBR_TYPE_UBPOOL; or, as a fallback,
 *      any partition that carries a valid pool label).
 *
 * This is the only piece the GUI tool adds below the bridge; the on-disk format
 * lives entirely in lib/ubixfs_core.  See docs/design/ubixfs-mac-browser-plan.md.
 */
#ifndef _UBFS_VDEV_IMAGE_H
#define _UBFS_VDEV_IMAGE_H

#include <ubfs_spa.h> /* ubfs_vdev_io_t, UBFS_* */

/* An opened image + the vdev view the core consumes.  Caller-allocated; it must
 * not be moved after a successful open (the io vtable's ctx points back at it). */
typedef struct ubfs_image
{
	int fd;              /* open file descriptor for the image */
	uint64_t base;       /* byte offset of the pool within the image */
	uint64_t span_bytes; /* usable bytes from base (partition or file size) */
	int rw;              /* opened read-write? */
	ubfs_vdev_io_t io;   /* the vtable handed to ubfs_pool_open (ctx = this) */
} ubfs_image_t;

/**
 * Open `path` and locate the UbixFS pool within it (raw or MBR-wrapped).  On
 * success fills *img with a ready-to-use vdev (img->io) and leaves the file
 * open.  Does NOT call ubfs_pool_open — the caller drives the core.
 *
 * @param rw  non-zero to open read-write; 0 for read-only.
 * @return 0 on success; -1 if the file cannot be opened; -2 if no UbixFS pool
 *         label is found anywhere in the image.
 */
int ubfs_image_open(ubfs_image_t *img, const char *path, int rw);

/** Close the image file (does not free img — it is caller-allocated). */
void ubfs_image_close(ubfs_image_t *img);

#endif /* _UBFS_VDEV_IMAGE_H */
