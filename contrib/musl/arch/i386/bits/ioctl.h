/* FreeBSD/UbixOS ioctl constants for i386.
 * Pull in generic musl (Linux-ABI) definitions, then remap the ones that
 * differ between Linux and FreeBSD/UbixOS.  The kernel uses FreeBSD ioctl
 * numbers; userland must match or the commands are silently ignored.
 *
 * Derivation:
 *   FreeBSD _IOC(dir,group,num,len) = dir | ((len&0x1fff)<<16) | (group<<8) | num
 *   IOC_OUT=0x40000000  IOC_IN=0x80000000
 *   sizeof(struct termios) = 36  (4×tcflag_t + 20×cc_t, NCCS=20)
 *   sizeof(struct winsize)  = 8  (4×unsigned short)
 *   sizeof(int)             = 4
 */
#include "../../generic/bits/ioctl.h"

/* termios get/set — Linux 0x5401-0x5404 → FreeBSD encoding */
#undef  TCGETS
#define TCGETS    0x40247413U  /* _IOR('t', 19, struct termios) */
#undef  TCSETS
#define TCSETS    0x80247414U  /* _IOW('t', 20, struct termios) */
#undef  TCSETSW
#define TCSETSW   0x80247415U  /* _IOW('t', 21, struct termios) */
#undef  TCSETSF
#define TCSETSF   0x80247416U  /* _IOW('t', 22, struct termios) */

/* window size */
#undef  TIOCGWINSZ
#define TIOCGWINSZ  0x40087468U  /* _IOR('t', 104, struct winsize) */
#undef  TIOCSWINSZ
#define TIOCSWINSZ  0x80087467U  /* _IOW('t', 103, struct winsize) */

/* foreground process group — Linux 0x540F/0x5410 → FreeBSD */
#undef  TIOCGPGRP
#define TIOCGPGRP  0x40047477U  /* _IOR('t', 119, int) */
#undef  TIOCSPGRP
#define TIOCSPGRP  0x80047476U  /* _IOW('t', 118, int) */
