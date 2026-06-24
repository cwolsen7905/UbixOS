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

/* termios get/set — FreeBSD encoding, sizeof(struct termios)=44=0x2C */
#undef  TCGETS
#define TCGETS    0x402C7413U  /* _IOR('t', 19, struct termios) */
#undef  TCSETS
#define TCSETS    0x802C7414U  /* _IOW('t', 20, struct termios) */
#undef  TCSETSW
#define TCSETSW   0x802C7415U  /* _IOW('t', 21, struct termios) */
#undef  TCSETSF
#define TCSETSF   0x802C7416U  /* _IOW('t', 22, struct termios) */

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

/* controlling terminal — Linux 0x540E/0x5422 → FreeBSD.  An ssh login (dropbear)
 * setsid()s its session child and then TIOCSCTTY's the pty slave; the Linux value
 * misses the kernel's FreeBSD case so the child can't acquire its tty and exits. */
#undef  TIOCSCTTY
#define TIOCSCTTY  0x20007461U  /* _IO('t', 97) */
#undef  TIOCNOTTY
#define TIOCNOTTY  0x20007471U  /* _IO('t', 113) */

/* line discipline — Linux 0x5424/0x5423 → FreeBSD (job-control shells call
 * TIOCGETD/TIOCSETD when grabbing the controlling tty; the kernel only knows
 * the FreeBSD encodings). */
#undef  TIOCGETD
#define TIOCGETD  0x4004741AU  /* _IOR('t', 26, int) */
#undef  TIOCSETD
#define TIOCSETD  0x8004741BU  /* _IOW('t', 27, int) */

/* bytes-ready — Linux 0x541B → FreeBSD.  busybox vi/less peek with FIONREAD to
 * tell a lone ESC from a cursor-key CSI; the Linux value misses the kernel's
 * FreeBSD case, so arrow keys break (ESC parsed as a command). */
#undef  FIONREAD
#define FIONREAD  0x4004667FU  /* _IOR('f', 127, int) */
