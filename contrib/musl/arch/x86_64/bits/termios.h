/* UbixOS/FreeBSD-ABI termios for i386.
 * Replaces the generic Linux layout. struct termios matches the kernel's
 * FreeBSD-style layout exactly so TIOCGETA/TIOCSETA ioctls work correctly.
 *
 * Kernel struct (sys/include/sys/ioctl.h, 44 bytes):
 *   tcflag_t c_iflag, c_oflag, c_cflag, c_lflag  (4×4=16)
 *   cc_t     c_cc[20]                             (20)
 *   speed_t  c_ispeed, c_ospeed                   (2×4=8)
 * Total: 44 bytes → TIOCGETA = _IOR('t',19,44) = 0x402C7413
 */

struct termios {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t     c_cc[NCCS];   /* NCCS=20 set in termios.h */
	speed_t  c_ispeed;
	speed_t  c_ospeed;
};

/* c_cc indices — FreeBSD order */
#define VEOF      0
#define VEOL      1
#define VEOL2     2
#define VERASE    3
#define VWERASE   4
#define VKILL     5
#define VREPRINT  6
/* 7: spare */
#define VINTR     8
#define VQUIT     9
#define VSUSP    10
#define VDSUSP   11
#define VSTART   12
#define VSTOP    13
#define VLNEXT   14
#define VDISCARD 15
#define VMIN     16
#define VTIME    17
#define VSTATUS  18
/* 19: spare */

/* c_iflag bits — FreeBSD hex values */
#define IGNBRK  0x00000001
#define BRKINT  0x00000002
#define IGNPAR  0x00000004
#define PARMRK  0x00000008
#define INPCK   0x00000010
#define ISTRIP  0x00000020
#define INLCR   0x00000040
#define IGNCR   0x00000080
#define ICRNL   0x00000100
#define IXON    0x00000200
#define IXOFF   0x00000400
#define IXANY   0x00000800
#define IMAXBEL 0x00002000
#define IUTF8   0x00004000

/* c_oflag bits */
#define OPOST   0x00000001
#define ONLCR   0x00000002
#define OCRNL   0x00000010
#define ONOCR   0x00000020
#define ONLRET  0x00000040
#define OFILL   0x00000080
#define OFDEL   0x00000100
#define NLDLY   0x00000300
#define NL0     0x00000000
#define NL1     0x00000100
#define TABDLY  0x00000004
#define TAB0    0x00000000
#define TAB3    0x00000004
#define XTABS   TAB3
#define CRDLY   0x00000000
#define CR0     0x00000000
#define BSDLY   0x00000000
#define BS0     0x00000000
#define FFDLY   0x00000000
#define FF0     0x00000000
#define VTDLY   0x00000000
#define VT0     0x00000000

/* c_cflag bits — FreeBSD hex values */
#define CSIZE   0x00000300
#define CS5     0x00000000
#define CS6     0x00000100
#define CS7     0x00000200
#define CS8     0x00000300
#define CSTOPB  0x00000400
#define CREAD   0x00000800
#define PARENB  0x00001000
#define PARODD  0x00002000
#define HUPCL   0x00004000
#define CLOCAL  0x00008000

/* c_lflag bits — FreeBSD hex values */
#define ECHOKE   0x00000001
#define ECHOE    0x00000002
#define ECHOK    0x00000004
#define ECHO     0x00000008
#define ECHONL   0x00000010
#define ECHOPRT  0x00000020
#define ECHOCTL  0x00000040
#define ISIG     0x00000080
#define ICANON   0x00000100
#define IEXTEN   0x00000400
#define TOSTOP   0x00400000
#define FLUSHO   0x00800000
#define PENDIN   0x20000000
#define NOFLSH   0x80000000

/* tcflow() constants */
#define TCOOFF  0
#define TCOON   1
#define TCIOFF  2
#define TCION   3

/* tcflush() constants */
#define TCIFLUSH  1
#define TCOFLUSH  2
#define TCIOFLUSH 3

/* tcsetattr() action constants */
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

/* Speed constants — abstract, same as Linux */
#define B0       0000000
#define B50      0000001
#define B75      0000002
#define B110     0000003
#define B134     0000004
#define B150     0000005
#define B200     0000006
#define B300     0000007
#define B600     0000010
#define B1200    0000011
#define B1800    0000012
#define B2400    0000013
#define B4800    0000014
#define B9600    0000015
#define B19200   0000016
#define B38400   0000017
#define B57600   0010001
#define B115200  0010002
#define B230400  0010003
#define B460800  0010004
#define B500000  0010005
#define B576000  0010006
#define B921600  0010007
#define B1000000 0010010
#define B1152000 0010011
#define B1500000 0010012
#define B2000000 0010013
#define B2500000 0010014
#define B3000000 0010015
#define B3500000 0010016
#define B4000000 0010017

/* cfget/cfset use c_ispeed/c_ospeed directly on BSD */
#define CBAUD    0
#define CBAUDEX  0
#define CIBAUD   0
#define CMSPAR   0
#define CRTSCTS  0x00010000

#if defined(_GNU_SOURCE) || defined(_BSD_SOURCE)
#define ALTWERASE  0x00000200
#define EXTPROC    0x00000800
#define NOKERNINFO 0x02000000
#define XCASE      0
#endif
