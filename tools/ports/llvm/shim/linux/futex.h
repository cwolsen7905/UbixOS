/* linux/futex.h — uBixOS/musl shim for libc++ (atomic.cpp uses the Linux futex
 * op constants; musl ships no <linux/futex.h> kernel header).  uBixOS has futex
 * (SYS_futex via musl -> sys_futex), so just the op constants are needed. */
#ifndef _UBIXOS_LINUX_FUTEX_H
#define _UBIXOS_LINUX_FUTEX_H
#define FUTEX_WAIT          0
#define FUTEX_WAKE          1
#define FUTEX_PRIVATE_FLAG  128
#define FUTEX_WAIT_PRIVATE  (FUTEX_WAIT | FUTEX_PRIVATE_FLAG)
#define FUTEX_WAKE_PRIVATE  (FUTEX_WAKE | FUTEX_PRIVATE_FLAG)
#endif
