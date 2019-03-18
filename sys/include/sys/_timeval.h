#ifndef _SYS__TIMEVAL_H
#define _SYS__TIMEVAL_H

#include <sys/_types.h>

#ifndef _SUSECONDS_T_DECLARED
typedef	__suseconds_t	suseconds_t;
#define	_SUSECONDS_T_DECLARED
#endif

#ifndef _TIME_T_DECLARED
typedef	__time_t	time_t;
#define	_TIME_T_DECLARED
#endif

// Structure returned by gettimeofday(2) system call, and used in other calls.
struct timeval {
	time_t		tv_sec;		/* seconds */
	suseconds_t	tv_usec;	/* and microseconds */
};

#endif /* END _SYS__TIMEVAL_H */
