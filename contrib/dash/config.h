/* config.h for dash on uBixOS (musl libc; identical for aarch64 and i386).
 * Written by hand from configure.ac's checks, since the tree is built without
 * autotools. */
#define _GNU_SOURCE 1
#define HAVE_ALIAS_ATTRIBUTE 1
#define HAVE_ALLOCA_H 1
#define HAVE_PATHS_H 1
#define HAVE_DECL_ISBLANK 1
#define SIZEOF_INTMAX_T 8
#define SIZEOF_LONG_LONG_INT 8
#define HAVE_BSEARCH 1
#define HAVE_FACCESSAT 1
#define HAVE_GETPWNAM 1
#define HAVE_GETRLIMIT 1
#define HAVE_ISALPHA 1
#define HAVE_KILLPG 1
#define HAVE_MEMPCPY 1
#define HAVE_STPCPY 1
#define HAVE_STRCHRNUL 1
#define HAVE_STRSIGNAL 1
#define HAVE_STRTOD 1
#define HAVE_STRTOIMAX 1
#define HAVE_STRTOUMAX 1
#define HAVE_SYSCONF 1
#define HAVE_FNMATCH 1
#define HAVE_ST_MTIM 1
/* musl >= 1.2.4 has no LFS64 names: the plain ones are already 64-bit. */
#define fstat64 fstat
#define lstat64 lstat
#define stat64 stat
#define glob64_t glob_t
#define glob64 glob
#define globfree64 globfree
#define open64 open
#define readdir64 readdir
#define dirent64 dirent
/* No line editing (no libedit); keep $LINENO. */
#define SMALL 1
#define WITH_LINENO 1
