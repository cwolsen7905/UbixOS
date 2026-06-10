/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * UbixFS in-kernel self-test (plan step K1).  Drives the portable lite-ZFS core
 * (lib/ubixfs_core) over a RAM-backed vdev — a single kmalloc'd buffer standing
 * in for a disk — to prove the core compiles and runs correctly inside the
 * freestanding kernel (kmalloc/kfree + the kernel string routines) with no block
 * device, partition, or VFS in play.  It mirrors the host milestone
 * tools/ubixfs/fs_test.c: format -> dsl create -> mkroot -> mkdir/create/write
 * -> read-back + attrs -> sync + reopen -> readdir/content intact.
 *
 * Result is logged to the console (PASS with a check count, or FAIL with the
 * failing count).  Called once from the aarch64 boot path before the real disk
 * mount; it de-risks the K2 read-only VFS mount and K3 write path.
 */
#include <sys/types.h>
#include <string.h> /* memcpy/memset/memcmp/strcmp (kernel sys/include/string.h) */
#include <lib/kprintf.h>
#include <lib/kmalloc.h>

#include <ubfs_fs.h>
#include <ubfs_dsl.h>

/* RAM pool + test file sizes.  2 MB pool = 512 blocks (ample for a few small
 * files); a 64 KB file exercises the DMU's multi-level indirect blkptr tree. */
#define POOL_BYTES (2u * 1024u * 1024u)
#define BIG_BYTES (64u * 1024u)

/* ── a RAM-backed vdev: the ubfs_vdev_io_t the core drives all I/O through ──── */
struct ram_vdev
{
	u_int8_t *base;
	uint64_t size_blocks;
};

/**
 * Read one UBFS_BLOCK_SIZE block @blk from the RAM vdev into @buf.
 *
 * @return 0 on success, -1 if @blk is past the end of the pool.
 */
static int ram_read(void *ctx, uint64_t blk, void *buf)
{
	struct ram_vdev *v = (struct ram_vdev *)ctx;

	if (blk >= v->size_blocks)
		return (-1);
	memcpy(buf, v->base + blk * UBFS_BLOCK_SIZE, UBFS_BLOCK_SIZE);
	return (0);
}

/**
 * Write one UBFS_BLOCK_SIZE block @blk from @buf into the RAM vdev.
 *
 * @return 0 on success, -1 if @blk is past the end of the pool.
 */
static int ram_write(void *ctx, uint64_t blk, const void *buf)
{
	struct ram_vdev *v = (struct ram_vdev *)ctx;

	if (blk >= v->size_blocks)
		return (-1);
	memcpy(v->base + blk * UBFS_BLOCK_SIZE, buf, UBFS_BLOCK_SIZE);
	return (0);
}

/* ── readdir collector: flags for the entry names we expect ─────────────────── */
struct dir_seen
{
	int bin, etc, motd, rc;
};

/**
 * readdir callback that records which expected names were seen.
 */
static int seen_cb(void *arg, const char *name, uint64_t obj, uint8_t type)
{
	struct dir_seen *s = (struct dir_seen *)arg;

	(void)obj;
	(void)type;
	if (strcmp(name, "bin") == 0)
		s->bin = 1;
	else if (strcmp(name, "etc") == 0)
		s->etc = 1;
	else if (strcmp(name, "motd") == 0)
		s->motd = 1;
	else if (strcmp(name, "rc") == 0)
		s->rc = 1;
	return (0);
}

/* Test bookkeeping. */
static int g_checks;
static int g_fails;

#define CHECK(cond, msg)                                                                                               \
	do                                                                                                             \
	{                                                                                                              \
		g_checks++;                                                                                            \
		if (!(cond))                                                                                           \
		{                                                                                                      \
			g_fails++;                                                                                     \
			kprintf("  FAIL: %s\n", (msg));                                                                \
		}                                                                                                      \
	} while (0)

/**
 * Run the UbixFS core over a RAM vdev and report PASS/FAIL to the console.
 *
 * Allocates a RAM pool + two scratch buffers, exercises the full
 * format/create/write/read/sync/reopen lifecycle, then frees everything.  Safe
 * to call once during bring-up; takes no arguments and returns nothing.
 */
void ubixfs_selftest(void)
{
	struct ram_vdev vdev;
	ubfs_vdev_io_t io;
	ubfs_pool_t *p;
	ubfs_dsl_t dsl;
	ubfs_dmu_os_t os;
	ubfs_dataset_phys_t dp;
	ubfs_fs_t fs;
	ubfs_inode_t in;
	uint64_t fs_ds, o, mo;
	u_int8_t *big, *rb;
	char buf[64];
	unsigned int i;

	g_checks = 0;
	g_fails = 0;
	kprintf("ubixfs: K1 in-kernel self-test (RAM vdev, %u KB pool)...\n", POOL_BYTES / 1024);

	vdev.base = (u_int8_t *)kmalloc(POOL_BYTES);
	big = (u_int8_t *)kmalloc(BIG_BYTES);
	rb = (u_int8_t *)kmalloc(BIG_BYTES);
	if (vdev.base == 0 || big == 0 || rb == 0)
	{
		kprintf("ubixfs: self-test SKIPPED — kmalloc failed (pool/scratch)\n");
		goto out;
	}
	memset(vdev.base, 0, POOL_BYTES);
	for (i = 0; i < BIG_BYTES; i++)
		big[i] = (u_int8_t)(i * 7 + 3);

	vdev.size_blocks = POOL_BYTES / UBFS_BLOCK_SIZE;
	io.ctx = &vdev;
	io.read = ram_read;
	io.write = ram_write;
	io.size_blocks = vdev.size_blocks;

	/* format + datasets + root */
	CHECK(ubfs_pool_format(&io, "tank", 1, &p) == 0, "format pool");
	CHECK(ubfs_dsl_create(p, &dsl) == 0, "create MOS");
	CHECK(ubfs_dsl_create_fs(&dsl, "root", &fs_ds) == 0, "create fs dataset");
	CHECK(ubfs_dsl_open_dataset(&dsl, fs_ds, &dp, &os) == 0, "open fs dataset");
	ubfs_fs_init(&fs, &os, 12345);
	CHECK(ubfs_fs_mkroot(&fs, 0, 0) == 0, "mkroot");
	CHECK(ubfs_fs_mkdir(&fs, "/bin", 0755, 0, 0, &o) == 0, "mkdir /bin");
	CHECK(ubfs_fs_mkdir(&fs, "/etc", 0755, 0, 0, &o) == 0, "mkdir /etc");

	/* files + perms */
	CHECK(ubfs_fs_create(&fs, "/etc/motd", 0644, 1000, 1000, &o) == 0, "create /etc/motd");
	CHECK(ubfs_fs_write(&fs, o, 0, "Welcome to uBixOS\n", 18) == 0, "write /etc/motd");
	CHECK(ubfs_fs_create(&fs, "/bin/sh", 0755, 0, 0, &o) == 0, "create /bin/sh");
	CHECK(ubfs_fs_write(&fs, o, 0, big, BIG_BYTES) == 0, "write 64KB into /bin/sh");
	CHECK(ubfs_fs_symlink(&fs, "/etc/rc", "/bin/sh", 0, 0, &o) == 0, "symlink /etc/rc -> /bin/sh");

	/* read back + attrs before sync */
	CHECK(ubfs_fs_lookup(&fs, "/etc/motd", &mo) == 0, "lookup /etc/motd");
	CHECK(ubfs_fs_getattr(&fs, mo, &in) == 0, "getattr /etc/motd");
	CHECK((in.mode & 07777) == 0644 && in.uid == 1000 && in.gid == 1000 && in.size == 18,
	      "/etc/motd mode/uid/gid/size");
	CHECK((int)ubfs_fs_read(&fs, mo, 0, buf, sizeof(buf)) == 18 && memcmp(buf, "Welcome to uBixOS\n", 18) == 0,
	      "/etc/motd content");
	CHECK(ubfs_fs_lookup(&fs, "/bin/sh", &mo) == 0 && ubfs_fs_read(&fs, mo, 0, rb, BIG_BYTES) == (int)BIG_BYTES &&
	          memcmp(rb, big, BIG_BYTES) == 0,
	      "/bin/sh 64KB content matches");

	/* sync + reopen (proves the on-disk image round-trips) */
	CHECK(ubfs_dsl_sync_dataset(&dsl, fs_ds, &os) == 0, "sync fs dataset");
	CHECK(ubfs_dsl_sync(&dsl) == 0, "sync MOS + commit");
	ubfs_pool_close(p);
	CHECK(ubfs_pool_open(&io, &p) == 0, "reopen pool");
	CHECK(ubfs_dsl_open(p, &dsl) == 0, "open MOS");
	CHECK(ubfs_dsl_open_dataset(&dsl, fs_ds, &dp, &os) == 0, "reopen fs dataset");
	ubfs_fs_init(&fs, &os, 99999);

	/* verify directory structure + content after reopen */
	{
		struct dir_seen root = {0, 0, 0, 0}, etc = {0, 0, 0, 0};
		ubfs_fs_readdir(&fs, UBFS_OBJ_ROOT, seen_cb, &root);
		CHECK(root.bin && root.etc, "readdir / has bin + etc");
		CHECK(ubfs_fs_lookup(&fs, "/etc", &mo) == 0, "lookup /etc");
		ubfs_fs_readdir(&fs, mo, seen_cb, &etc);
		CHECK(etc.motd && etc.rc, "readdir /etc has motd + rc");
		CHECK(ubfs_fs_lookup(&fs, "/etc/motd", &mo) == 0 && ubfs_fs_read(&fs, mo, 0, buf, sizeof(buf)) == 18 &&
		          memcmp(buf, "Welcome to uBixOS\n", 18) == 0,
		      "/etc/motd content intact across reopen");
	}

	ubfs_pool_close(p);

	if (g_fails == 0)
		kprintf("ubixfs: K1 self-test PASS (%d checks) — core is kernel-viable\n", g_checks);
	else
		kprintf("ubixfs: K1 self-test FAIL (%d/%d checks failed)\n", g_fails, g_checks);

out:
	if (rb != 0)
		kfree(rb);
	if (big != 0)
		kfree(big);
	if (vdev.base != 0)
		kfree(vdev.base);
}
