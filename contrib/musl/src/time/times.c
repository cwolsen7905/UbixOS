#include <sys/times.h>
#include <sys/resource.h>
#include <time.h>
#include "syscall.h"

/* UbixOS: no times(2) syscall (FreeBSD has none either).  Build it from
 * getrusage and the monotonic clock in CLK_TCK (100 Hz) units, as FreeBSD
 * libc does.  Linux's SYS_times number landed on getegid. */
#define TV_TO_TICKS(tv) ((clock_t)(tv).tv_sec * 100 + (tv).tv_usec / 10000)

clock_t times(struct tms *tms)
{
	struct rusage self, kids;
	struct timespec now;

	if (getrusage(RUSAGE_SELF, &self) || getrusage(RUSAGE_CHILDREN, &kids)
	    || clock_gettime(CLOCK_MONOTONIC, &now))
		return (clock_t)-1;
	if (tms) {
		tms->tms_utime = TV_TO_TICKS(self.ru_utime);
		tms->tms_stime = TV_TO_TICKS(self.ru_stime);
		tms->tms_cutime = TV_TO_TICKS(kids.ru_utime);
		tms->tms_cstime = TV_TO_TICKS(kids.ru_stime);
	}
	return (clock_t)now.tv_sec * 100 + now.tv_nsec / 10000000;
}
