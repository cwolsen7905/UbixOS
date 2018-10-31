#ifndef _DIR_H
#define _DIR_H

/**
 * Size of directory block
 */
#define	DIRBLKSIZE	512

#ifndef DIRSIZ
#define DIRSIZ 14
#endif

typedef struct __DIR__DIRECT_
{
	unsigned int d_ino; /* defined as __kernel_ino_t in posix_types.h */
	char d_name[DIRSIZ];
} direct;

#endif /* _DIR_H */
