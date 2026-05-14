/* UbixOS i386 fcntl flags — FreeBSD ABI values */

#define O_CREAT         0x0200
#define O_EXCL          0x0800
#define O_NOCTTY        0x8000
#define O_TRUNC         0x0400
#define O_APPEND        0x0008
#define O_NONBLOCK      0x0004
#define O_DSYNC         0x1000
#define O_SYNC          0x0080
#define O_RSYNC         0x0080
#define O_DIRECTORY     0x00010000
#define O_NOFOLLOW      0x0100
#define O_CLOEXEC       0x00100000

#define O_ASYNC         0x0040
#define O_DIRECT        0x4000
#define O_LARGEFILE     0x8000
#define O_NOATIME       0
#define O_PATH          0
#define O_TMPFILE       0
#define O_NDELAY        O_NONBLOCK

#define F_DUPFD  0
#define F_GETFD  1
#define F_SETFD  2
#define F_GETFL  3
#define F_SETFL  4
#define F_GETOWN 5
#define F_SETOWN 6
#define F_GETLK  7
#define F_SETLK  8
#define F_SETLKW 9

#define F_SETOWN_EX     20
#define F_GETOWN_EX     21
#define F_GETOWNER_UIDS 22
