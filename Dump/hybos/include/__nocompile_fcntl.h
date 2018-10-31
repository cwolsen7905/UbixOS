/**
 * fcntl.h
 *
 * access constants for _open. Note that this file is not complete
 * and should not be used (yet)
 */

#ifndef __STRICT_ANSI__
#ifndef _FCNTL_H

#define _FCNTL_H

/**
 * File access modes
 */
#define	_O_RDONLY	0 /* read only */
#define	_O_WRONLY	1 /* write only */
#define	_O_RDWR		2 /* read and write */

/**
 * Mask for access mode bits in the _open flags
 */
#define	_O_ACCMODE		(_O_RDONLY|_O_WRONLY|_O_RDWR)
#define	_O_APPEND		0x0008 /* append output to end of file */
#define	_O_RANDOM		0x0010
#define	_O_SEQUENTIAL	0x0020
#define	_O_TEMPORARY	0x0040 /* make the file dissappear after closing */
#define	_O_NOINHERIT	0x0080
#define	_O_CREAT			0x0100 /* create file if it doesn't exist */
#define	_O_TRUNC			0x0200 /* truncate file to zero bytes if it exists */
#define	_O_EXCL			0x0400 /* open only if the file does not exist */

#define	_O_SHORT_LIVED	0x1000

#define	_O_TEXT			0x4000 /* crlf in file == lf in memory (\r\n == \n) */
#define	_O_BINARY		0x8000 /* input and output is not translated */
#define	_O_RAW			_O_BINARY /* compatability */

/**
 * XXX
 * The following special mode(s) are only available to the
 * kernel. Modules (even kernel-level modules) and other
 * binaries will never be allowed to use these special modes.
 *
 * Modules which are compiled into the kernel may use these
 * special modes (mainly at boot).
 */
#define	_O_DEV			0x9000 /* XXX: special device links (files) */
#define	_O_SWAP			0x9100 /* swap */

#ifndef _NO_OLDNAMES

/**
 * POSIX/Non-ANSI names for increased portability
 */
#define	O_RDONLY			_O_RDONLY
#define	O_WRONLY			_O_WRONLY
#define	O_RDWR			_O_RDWR
#define	O_ACCMODE		_O_ACCMODE
#define	O_APPEND			_O_APPEND
#define	O_CREAT			_O_CREAT
#define	O_TRUNC			_O_TRUNC
#define	O_EXCL			_O_EXCL
#define	O_TEXT			_O_TEXT
#define	O_BINARY			_O_BINARY
#define	O_TEMPORARY		_O_TEMPORARY
#define	O_NOINHERIT		_O_NOINHERIT
#define	O_SEQUENTIAL	_O_SEQUENTIAL
#define	O_RANDOM			_O_RANDOM

#define	O_DEV				_O_DEV
#define	O_SWAP			_O_SWAP

#endif /* ! _NO_OLDNAMES */

#endif /* ! _FCNTL_H */

#endif /* ! __STRICT_ANSI__ */

