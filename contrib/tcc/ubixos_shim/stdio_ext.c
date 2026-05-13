/*
 * Missing stdio functions for TCC on UbixOS
 * Copyright (c) 2002-2026 The UbixOS Project.
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

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


int sscanf(const char *buf, const char *fmt, ...)
{
	(void)buf; (void)fmt;
	return 0; /* stub */
}
