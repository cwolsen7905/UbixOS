#include <sys/time.h>

extern int _sys_gettimeofday(struct timeval *tp, struct timezone *tzp);

int gettime(void) {
    struct timeval tv = { 0, 0 };
    struct timezone tz = { 0, 0 };
    _sys_gettimeofday(&tv, &tz);
    return (int)tv.tv_sec;
}
