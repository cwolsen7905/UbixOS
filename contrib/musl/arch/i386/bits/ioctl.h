/* FreeBSD/UbixOS ioctl constants for i386.
 * Pull in all generic musl ioctl definitions, then override TIOCGWINSZ
 * which differs between Linux (0x5413) and FreeBSD/UbixOS (0x40087468). */
#include "../../generic/bits/ioctl.h"
#undef  TIOCGWINSZ
#define TIOCGWINSZ  0x40087468U
