#define LENGTH 20
/* shahl.c
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: releng/11.1/lib/libmd/shahl.c 310372 2016-12-21 18:42:04Z emaste $");

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "sha.h"

char *
SHA1_End(SHA1_CTX *ctx, char *buf)
{
	int i;
	unsigned char digest[LENGTH];
	static const char hex[]="0123456789abcdef";

	if (!buf)
		buf = malloc(2*LENGTH + 1);
	if (!buf)
		return 0;
	SHA1_Final(digest, ctx);
	for (i = 0; i < LENGTH; i++) {
		buf[i+i] = hex[digest[i] >> 4];
		buf[i+i+1] = hex[digest[i] & 0x0f];
	}
	buf[i+i] = '\0';
	return buf;
}

char *
SHA1_Fd(int fd, char *buf)
{
	return SHA1_FdChunk(fd, buf, 0, 0);
}

char *
SHA1_FdChunk(int fd, char *buf, off_t ofs, off_t len)
{
	unsigned char buffer[16*1024];
	SHA1_CTX ctx;
	struct stat stbuf;
	int readrv, e;
	off_t remain;

	if (len < 0) {
		errno = EINVAL;
		return NULL;
	}

	SHA1_Init(&ctx);
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
		SHA1_Update(&ctx, buffer, readrv);
		remain -= readrv;
	} 
error:
	if (readrv < 0)
		return NULL;
	return (SHA1_End(&ctx, buf));
}

char *
SHA1_File(const char *filename, char *buf)
{
	return (SHA1_FileChunk(filename, buf, 0, 0));
}

char *
SHA1_FileChunk(const char *filename, char *buf, off_t ofs, off_t len)
{
	char *ret;
	int e, fd;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return NULL;
	ret = SHA1_FdChunk(fd, buf, ofs, len);
	e = errno;
	close (fd);
	errno = e;
	return ret;
}

char *
SHA1_Data (const void *data, unsigned int len, char *buf)
{
	SHA1_CTX ctx;

	SHA1_Init(&ctx);
	SHA1_Update(&ctx,data,len);
	return (SHA1_End(&ctx, buf));
}

#ifdef WEAK_REFS
/* When building libmd, provide weak references. Note: this is not
   activated in the context of compiling these sources for internal
   use in libcrypt.
 */
#undef SHA1_End
__weak_reference(_libmd_SHA1_End, SHA1_End);
#undef SHA1_File
__weak_reference(_libmd_SHA1_File, SHA1_File);
#undef SHA1_FileChunk
__weak_reference(_libmd_SHA1_FileChunk, SHA1_FileChunk);
#undef SHA1_Data
__weak_reference(_libmd_SHA1_Data, SHA1_Data);
#endif
