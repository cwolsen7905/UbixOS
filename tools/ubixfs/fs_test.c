/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * fs_test — host milestone for the UbixFS POSIX layer (ubfs_fs):
 *   make a pool + fs dataset -> mkroot -> mkdir/create/write/symlink ->
 *   read/getattr/readdir -> sync + reopen -> everything intact -> unlink.
 */
#include <ubfs_fs.h>
#include <ubfs_dsl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct fdev
{
	int fd;
};
static int fdr(void *c, uint64_t b, void *buf)
{
	return pread(((struct fdev *)c)->fd, buf, UBFS_BLOCK_SIZE, (off_t)b * UBFS_BLOCK_SIZE) == UBFS_BLOCK_SIZE ? 0
	                                                                                                          : -1;
}
static int fdw(void *c, uint64_t b, const void *buf)
{
	return pwrite(((struct fdev *)c)->fd, buf, UBFS_BLOCK_SIZE, (off_t)b * UBFS_BLOCK_SIZE) == UBFS_BLOCK_SIZE ? 0
	                                                                                                           : -1;
}

static int fails;
#define CHECK(c, m)                                                                                                  \
	do {                                                                                                           \
		int _ok = (c);                                                                                          \
		printf("  %s : %s\n", _ok ? "ok  " : "FAIL", m);                                                        \
		if (!_ok)                                                                                                \
			fails++;                                                                                       \
	} while (0)

/* readdir collector: concatenates "name " for live entries */
struct collect
{
	char buf[256];
};
static int collect_cb(void *arg, const char *name, uint64_t obj, uint8_t type)
{
	struct collect *c = (struct collect *)arg;
	(void)obj;
	(void)type;
	strncat(c->buf, name, sizeof(c->buf) - strlen(c->buf) - 2);
	strncat(c->buf, " ", sizeof(c->buf) - strlen(c->buf) - 1);
	return 0;
}

#define BIG 200000

int main(void)
{
	const char    *path = "/tmp/ubfs_fs.img";
	struct fdev    d;
	ubfs_vdev_io_t io;
	ubfs_pool_t   *p;
	ubfs_dsl_t     dsl;
	ubfs_dmu_os_t  os;
	ubfs_dataset_phys_t dp;
	ubfs_fs_t      fs;
	uint64_t       fs_ds, o;
	uint8_t       *big = malloc(BIG), *rb = malloc(BIG);

	d.fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (d.fd < 0 || ftruncate(d.fd, 64ll * 1024 * 1024) < 0) {
		perror("img");
		return 2;
	}
	io.ctx = &d;
	io.read = fdr;
	io.write = fdw;
	io.size_blocks = 64ull * 1024 * 1024 / UBFS_BLOCK_SIZE;
	for (int i = 0; i < BIG; i++)
		big[i] = (uint8_t)(i * 7 + 3);

	CHECK(ubfs_pool_format(&io, "tank", 1, &p) == 0, "format pool");
	CHECK(ubfs_dsl_create(p, &dsl) == 0, "create MOS");
	CHECK(ubfs_dsl_create_fs(&dsl, "root", &fs_ds) == 0, "create fs dataset");
	CHECK(ubfs_dsl_open_dataset(&dsl, fs_ds, &dp, &os) == 0, "open fs dataset");

	ubfs_fs_init(&fs, &os, 12345);
	CHECK(ubfs_fs_mkroot(&fs, 0, 0) == 0, "mkroot");
	CHECK(ubfs_fs_mkdir(&fs, "/bin", 0755, 0, 0, &o) == 0, "mkdir /bin");
	CHECK(ubfs_fs_mkdir(&fs, "/etc", 0755, 0, 0, &o) == 0, "mkdir /etc");

	printf("== files + perms ==\n");
	CHECK(ubfs_fs_create(&fs, "/etc/motd", 0644, 1000, 1000, &o) == 0, "create /etc/motd");
	CHECK(ubfs_fs_write(&fs, o, 0, "Welcome to uBixOS\n", 18) == 0, "write /etc/motd");
	CHECK(ubfs_fs_create(&fs, "/bin/sh", 0755, 0, 0, &o) == 0, "create /bin/sh");
	CHECK(ubfs_fs_write(&fs, o, 0, big, BIG) == 0, "write 200KB into /bin/sh");
	CHECK(ubfs_fs_symlink(&fs, "/etc/rc", "/bin/sh", 0, 0, &o) == 0, "symlink /etc/rc -> /bin/sh");

	printf("== read back + attrs (before sync) ==\n");
	{
		ubfs_inode_t in;
		char         buf[64];
		uint64_t     mo;
		CHECK(ubfs_fs_lookup(&fs, "/etc/motd", &mo) == 0, "lookup /etc/motd");
		CHECK(ubfs_fs_getattr(&fs, mo, &in) == 0, "getattr /etc/motd");
		CHECK((in.mode & 07777) == 0644 && in.uid == 1000 && in.gid == 1000 && in.size == 18,
		      "/etc/motd mode 0644 uid/gid 1000 size 18");
		CHECK((int)ubfs_fs_read(&fs, mo, 0, buf, sizeof(buf)) == 18 && memcmp(buf, "Welcome to uBixOS\n", 18) == 0,
		      "/etc/motd content");
		CHECK(ubfs_fs_lookup(&fs, "/bin/sh", &mo) == 0 && ubfs_fs_read(&fs, mo, 0, rb, BIG) == BIG &&
		          memcmp(rb, big, BIG) == 0,
		      "/bin/sh 200KB content matches");
		CHECK(ubfs_fs_lookup(&fs, "/etc/rc", &mo) == 0 && ubfs_fs_readlink(&fs, mo, buf, sizeof(buf)) == 7 &&
		          strcmp(buf, "/bin/sh") == 0,
		      "symlink /etc/rc target");
	}

	printf("== sync + reopen ==\n");
	CHECK(ubfs_dsl_sync_dataset(&dsl, fs_ds, &os) == 0, "sync fs dataset");
	CHECK(ubfs_dsl_sync(&dsl) == 0, "sync MOS + commit");
	ubfs_pool_close(p);
	CHECK(ubfs_pool_open(&io, &p) == 0, "reopen pool");
	CHECK(ubfs_dsl_open(p, &dsl) == 0, "open MOS");
	CHECK(ubfs_dsl_open_dataset(&dsl, fs_ds, &dp, &os) == 0, "reopen fs dataset");
	ubfs_fs_init(&fs, &os, 99999);

	printf("== verify after reopen ==\n");
	{
		struct collect root = {{0}}, etc = {{0}};
		uint64_t       mo;
		char           buf[64];
		ubfs_fs_readdir(&fs, UBFS_OBJ_ROOT, collect_cb, &root);
		CHECK(strstr(root.buf, "bin") && strstr(root.buf, "etc"), "readdir / has bin + etc");
		CHECK(ubfs_fs_lookup(&fs, "/etc", &mo) == 0, "lookup /etc");
		ubfs_fs_readdir(&fs, mo, collect_cb, &etc);
		CHECK(strstr(etc.buf, "motd") && strstr(etc.buf, "rc"), "readdir /etc has motd + rc");
		CHECK(ubfs_fs_lookup(&fs, "/etc/motd", &mo) == 0 &&
		          ubfs_fs_read(&fs, mo, 0, buf, sizeof(buf)) == 18 &&
		          memcmp(buf, "Welcome to uBixOS\n", 18) == 0,
		      "/etc/motd content intact across reopen");
		CHECK(ubfs_fs_lookup(&fs, "/bin/sh", &mo) == 0 && ubfs_fs_read(&fs, mo, 0, rb, BIG) == BIG &&
		          memcmp(rb, big, BIG) == 0,
		      "/bin/sh 200KB intact across reopen");
	}

	printf("== unlink ==\n");
	{
		struct collect etc = {{0}};
		uint64_t       mo;
		CHECK(ubfs_fs_unlink(&fs, "/etc/motd") == 0, "unlink /etc/motd");
		CHECK(ubfs_fs_lookup(&fs, "/etc/motd", &mo) != 0, "/etc/motd gone");
		ubfs_fs_lookup(&fs, "/etc", &mo);
		ubfs_fs_readdir(&fs, mo, collect_cb, &etc);
		CHECK(!strstr(etc.buf, "motd") && strstr(etc.buf, "rc"), "readdir /etc now only rc");
	}
	ubfs_pool_close(p);

	free(big);
	free(rb);
	close(d.fd);
	printf("\n%s\n", fails ? "FS TEST: FAIL" : "FS TEST: PASS");
	return fails ? 1 : 0;
}
