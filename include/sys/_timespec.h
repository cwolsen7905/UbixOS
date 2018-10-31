#ifndef _SYS__TIMESPEC_H
#define _SYS__TIMESPEC_H

#include <sys/_types.h>

#ifndef _TIME_T_DECLARED
typedef __time_t        time_t;
#define _TIME_T_DECLARED
#endif

struct timespec {
        time_t  tv_sec;         /* seconds */
        long    tv_nsec;        /* and nanoseconds */
};

#endif /* END _SYS__TIMESPEC_H */

