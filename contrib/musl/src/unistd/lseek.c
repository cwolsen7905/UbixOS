#include <unistd.h>
#include "syscall.h"

off_t __lseek(int fd, off_t offset, int whence)
{
	/* Use __SYSCALL_LL_O to split the 64-bit offset into two 32-bit args so
	 * the token-counting __syscall macro picks __syscall4 and the FreeBSD
	 * kernel struct (fd, off_lo, off_hi, whence) is filled correctly. */
	return syscall(SYS_lseek, fd, __SYSCALL_LL_O(offset), whence);
}

weak_alias(__lseek, lseek);
