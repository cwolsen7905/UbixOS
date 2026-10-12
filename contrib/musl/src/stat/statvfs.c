#include <sys/statvfs.h>
#include <sys/statfs.h>
#include <stdint.h>
#include "syscall.h"

/* UbixOS: the kernel's statfs/fstatfs are FreeBSD's (slots 396/397: path or fd,
 * buf) and fill FreeBSD's struct statfs (MNAMELEN 88).  musl's callers expect
 * Linux's struct statfs, so take FreeBSD's and convert.  (Linux's 3-argument
 * statfs64 form put the buffer size where the kernel expects the buffer.) */
struct ubix_kstatfs {
	uint32_t f_version, f_type;
	uint64_t f_flags, f_bsize, f_iosize, f_blocks, f_bfree;
	int64_t f_bavail;
	uint64_t f_files;
	int64_t f_ffree;
	uint64_t f_syncwrites, f_asyncwrites, f_syncreads, f_asyncreads, f_spare[10];
	uint32_t f_namemax, f_owner;
	int32_t f_fsid[2];
	char f_charspare[80], f_fstypename[16], f_mntfromname[88], f_mntonname[88];
};

#define UBIX_MNT_RDONLY 0x1
#define UBIX_MNT_NOSUID 0x8

static void from_kstatfs(struct statfs *out, const struct ubix_kstatfs *k)
{
	*out = (struct statfs){0};
	out->f_type = k->f_type;
	out->f_bsize = k->f_bsize;
	out->f_frsize = k->f_bsize;
	out->f_blocks = k->f_blocks;
	out->f_bfree = k->f_bfree;
	out->f_bavail = k->f_bavail < 0 ? 0 : k->f_bavail;
	out->f_files = k->f_files;
	out->f_ffree = k->f_ffree < 0 ? 0 : k->f_ffree;
	out->f_fsid.__val[0] = k->f_fsid[0];
	out->f_fsid.__val[1] = k->f_fsid[1];
	out->f_namelen = k->f_namemax;
	out->f_flags = ((k->f_flags & UBIX_MNT_RDONLY) ? ST_RDONLY : 0)
	             | ((k->f_flags & UBIX_MNT_NOSUID) ? ST_NOSUID : 0);
}

static int __statfs(const char *path, struct statfs *buf)
{
	struct ubix_kstatfs k;
	int r = syscall(SYS_statfs64, path, &k);
	if (!r) from_kstatfs(buf, &k);
	return r;
}

static int __fstatfs(int fd, struct statfs *buf)
{
	struct ubix_kstatfs k;
	int r = syscall(SYS_fstatfs64, fd, &k);
	if (!r) from_kstatfs(buf, &k);
	return r;
}

weak_alias(__statfs, statfs);
weak_alias(__fstatfs, fstatfs);

static void fixup(struct statvfs *out, const struct statfs *in)
{
	*out = (struct statvfs){0};
	out->f_bsize = in->f_bsize;
	out->f_frsize = in->f_frsize ? in->f_frsize : in->f_bsize;
	out->f_blocks = in->f_blocks;
	out->f_bfree = in->f_bfree;
	out->f_bavail = in->f_bavail;
	out->f_files = in->f_files;
	out->f_ffree = in->f_ffree;
	out->f_favail = in->f_ffree;
	out->f_fsid = in->f_fsid.__val[0];
	out->f_flag = in->f_flags;
	out->f_namemax = in->f_namelen;
	out->f_type = in->f_type;
}

int statvfs(const char *restrict path, struct statvfs *restrict buf)
{
	struct statfs kbuf;
	if (__statfs(path, &kbuf)<0) return -1;
	fixup(buf, &kbuf);
	return 0;
}

int fstatvfs(int fd, struct statvfs *buf)
{
	struct statfs kbuf;
	if (__fstatfs(fd, &kbuf)<0) return -1;
	fixup(buf, &kbuf);
	return 0;
}
