#define LENGTH 16
/* md4hl.c
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: releng/11.1/lib/libmd/md4hl.c 310372 2016-12-21 18:42:04Z emaste $");

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "md4.h"

char *
MD4End(MD4_CTX *ctx, char *buf)
{
	int i;
	unsigned char digest[LENGTH];
	static const char hex[]="0123456789abcdef";

	if (!buf)
		buf = malloc(2*LENGTH + 1);
	if (!buf)
		return 0;
	MD4Final(digest, ctx);
	for (i = 0; i < LENGTH; i++) {
		buf[i+i] = hex[digest[i] >> 4];
		buf[i+i+1] = hex[digest[i] & 0x0f];
	}
	buf[i+i] = '\0';
	return buf;
}

char *
MD4Fd(int fd, char *buf)
{
	return MD4FdChunk(fd, buf, 0, 0);
}

char *
MD4FdChunk(int fd, char *buf, off_t ofs, off_t len)
{
	unsigned char buffer[16*1024];
	MD4_CTX ctx;
	struct stat stbuf;
	int readrv, e;
	off_t remain;

	if (len < 0) {
		errno = EINVAL;
		return NULL;
	}

	MD4Init(&ctx);
	if (ofs != 0) {
		errno = 0;
		if (lseek(fd, ofs, SEEK_SET) != ofs ||
		    (ofs == -1 && errno != 0)) {
			readrv = -1;
			goto error;
		}
	}
	remain = len;
	readrv = 0;
	while (len == 0 || remain > 0) {
		if (len == 0 || remain > sizeof(buffer))
			readrv = read(fd, buffer, sizeof(buffer));
		else
			readrv = read(fd, buffer, remain);
		if (readrv <= 0) 
			break;
		MD4Update(&ctx, buffer, readrv);
		remain -= readrv;
	} 
error:
	if (readrv < 0)
		return NULL;
	return (MD4End(&ctx, buf));
}

char *
MD4File(const char *filename, char *buf)
{
	return (MD4FileChunk(filename, buf, 0, 0));
}

char *
MD4FileChunk(const char *filename, char *buf, off_t ofs, off_t len)
{
	char *ret;
	int e, fd;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return NULL;
	ret = MD4FdChunk(fd, buf, ofs, len);
	e = errno;
	close (fd);
	errno = e;
	return ret;
}

char *
MD4Data (const void *data, unsigned int len, char *buf)
{
	MD4_CTX ctx;

	MD4Init(&ctx);
	MD4Update(&ctx,data,len);
	return (MD4End(&ctx, buf));
}

#ifdef WEAK_REFS
/* When building libmd, provide weak references. Note: this is not
   activated in the context of compiling these sources for internal
   use in libcrypt.
 */
#undef MD4End
__weak_reference(_libmd_MD4End, MD4End);
#undef MD4File
__weak_reference(_libmd_MD4File, MD4File);
#undef MD4FileChunk
__weak_reference(_libmd_MD4FileChunk, MD4FileChunk);
#undef MD4Data
__weak_reference(_libmd_MD4Data, MD4Data);
#endif
