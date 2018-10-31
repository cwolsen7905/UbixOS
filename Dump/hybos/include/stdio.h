/**
 * stdio.h
 *
 * Input/output functions
 */

#ifndef _STDIO_H
#define _STDIO_H

#define	SEEK_SET	0
#define	SEEK_CUR	1
#define	SEEK_END	2

#ifndef _NULL
#define	NULL	((void *)0)
#endif
#define	EOF	(-1)

#define	FOPEN_MAX		20
#define	FILENAME_MAX	14
#define	TMP_MAX			999

typedef long int	fpos_t;

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

/*int remove(const char *_filename);*/
/*int rename(const char *_old, const char *_new);*/
/*FILE *tmpfile(void);*/
/*char *tmpnam(char *_s);*/
/*int fclose(FILE *_stream);*/
/*int fflush(FILE *_stream);*/
/*FILE *fopen(const char *_filename, const char *_mode);*/
/*FILE *freopen(const char *_filename, const char *_mode, FILE *_stream);*/
/*void setbuf(FILE *_stream, char *_buf);*/
/*int setvbuf(FILE *_stream, char *_buf, int _mode, size_t _size);*/
/*int fprintf(FILE *_stream, const char *_format, ...);*/
/*int printf(const char *fmt, ...);*/
void printf(const char *format, ...);
int sprintf(char *s, const char *format, ...);
/*int vfprintf(FILE *_stream, const char *_format, char *_arg);*/
/*int vprintf(const char *_format, char *_arg);*/
int vsprintf(char *s, const char *format, ...);
/*int fscanf(FILE *_stream, const char *_format, ...);*/
/*int scanf(const char *_format, ...);*/
/*int sscanf(const char *_s, const char *_format, ...);*/
/*#define vfscanf _doscan*/
/*int vfscanf(FILE *_stream, const char *_format, char *_arg);*/
/*int vscanf(const char *_format, char *_arg);*/
/*int vsscanf(const char *_s, const char *_format, char *_arg);*/
/*int fgetc(FILE *_stream);*/
/*char *fgets(char *_s, int _n, FILE *_stream);*/
/*int fputc(int _c, FILE *_stream);*/
/*int fputs(const char *_s, FILE *_stream);*/
/*int getc(FILE *_stream);*/
/*int getchar(void);*/
/*char *gets(char *_s);*/
/*int putc(int _c, FILE *_stream);*/
/*int putchar(int _c);*/
/*int puts(const char *_s);*/
/*int ungetc(int _c, FILE *_stream);*/
/*size_t fread(void *_ptr, size_t _size, size_t _nmemb, FILE *_stream);*/
/*size_t fwrite(const void *_ptr, size_t _size, size_t _nmemb, FILE *_stream);*/
/*int fgetpos(FILE *_stream, fpos_t *_pos);*/
/*int fseek(FILE *_stream, long _offset, int _whence);*/
/*int fsetpos(FILE *_stream, fpos_t *_pos);*/
/*long ftell(FILE *_stream);*/
/*void rewind(FILE *_stream);*/
/*void clearerr(FILE *_stream);*/
/*int feof(FILE *_stream);*/
/*int ferror(FILE *_stream);*/
/*void perror(const char *_s);*/
/*int __fillbuf(FILE *_stream);*/
/*int __flushbuf(int _c, FILE *_stream);*/

#endif /* _STDIO_H */
