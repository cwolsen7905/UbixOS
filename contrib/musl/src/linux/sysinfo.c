#include <sys/sysinfo.h>
#include <string.h>
#include "syscall.h"

int __lsysinfo(struct sysinfo *info)
{
	/* UbixOS: SYS_sysinfo is stubbed (ENOSYS).  Callers such as sysconf
	 * read the struct even when this fails, so never leave it uninitialised. */
	memset(info, 0, sizeof *info);
	return syscall(SYS_sysinfo, info);
}

weak_alias(__lsysinfo, sysinfo);
