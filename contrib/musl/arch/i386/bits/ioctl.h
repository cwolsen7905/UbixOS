/* FreeBSD/UbixOS ioctl constants for i386 — override musl's Linux defaults */

/* TIOCGWINSZ: FreeBSD _IOR('t', 104, struct winsize) = 0x40087468
 * Linux uses 0x5413 — kernel only handles the FreeBSD value */
#define TIOCGWINSZ   0x40087468
