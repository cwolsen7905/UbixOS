/*
 * Missing stdio functions for TCC on UbixOS
 * Copyright (c) 2002-2026 The UbixOS Project.
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

/* vsprintf is already in libc; use it for vsnprintf with truncation */
extern int vsprintf(char *buf, const char *fmt, va_list args);

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	char tmp[4096];
	int n;
	if (size == 0)
		return 0;
	n = vsprintf(tmp, fmt, ap);
	if (n < 0)
		n = 0;
	if ((size_t)n >= size)
		n = (int)size - 1;
	memcpy(buf, tmp, n);
	buf[n] = '\0';
	return n;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list ap;
	int n;
	va_start(ap, fmt);
	n = vsnprintf(buf, size, fmt, ap);
	va_end(ap);
	return n;
}

int vprintf(const char *fmt, va_list ap)
{
	return vfprintf(stdout, fmt, ap);
}

int fputc(int c, FILE *fp)
{
	char ch = (char)c;
	fwrite(&ch, 1, 1, fp);
	return (unsigned char)c;
}

int fputs(const char *s, FILE *fp)
{
	size_t len = strlen(s);
	return (int)fwrite(s, 1, len, fp);
}

long ftell(FILE *fp)
{
	/* delegate to fseek(fp, 0, SEEK_CUR) */
	fseek(fp, 0, 1); /* SEEK_CUR = 1 */
	return (long)fp->fd; /* placeholder — real impl needs kernel offset */
}

int ferror(FILE *fp)
{
	(void)fp;
	return 0;
}

void rewind(FILE *fp)
{
	fseek(fp, 0, 0); /* SEEK_SET */
}

int remove(const char *path)
{
	(void)path;
	return -1; /* not implemented */
}

void perror(const char *s)
{
	if (s && *s) {
		fputs(s, stderr);
		fputs(": error\n", stderr);
	}
}

/* fdopen: wrap a raw file descriptor in a FILE* */
FILE *fdopen(int fd, const char *mode)
{
	/* Our FILE struct is {u_long fd; uint32_t size} */
	FILE *fp = malloc(sizeof(FILE));
	if (!fp)
		return NULL;
	fp->fd   = (unsigned long)fd;
	fp->size = 0;
	(void)mode;
	return fp;
}

int sscanf(const char *buf, const char *fmt, ...)
{
	(void)buf; (void)fmt;
	return 0; /* stub */
}
