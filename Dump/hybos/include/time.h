/**
 * time.h
 */

#ifndef _TIME_H
#define _TIME_H

#define	CLOCKS_PER_SEC		60		/* 60 Hz */

#ifndef NULL
#define NULL ((void *) 0)
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

#ifndef _TIME_T
#define _TIME_T
typedef long time_t;		/* time in seconds since January 1, 1970 0000 GMT */
#endif

#ifndef _CLOCK_T
#define _CLOCK_T
typedef long clock_t;	/* time in ticks since process started */
#endif

typedef struct __TIME_H_TIME
{
	int tm_sec;		/* seconds after the minute [0, 59] */
	int tm_min;		/* minutes after the hour [0, 59] */
	int tm_hour;	/* hours since midnight [0, 23] */
	int tm_mday;	/* day of the month [1, 31] */
	int tm_mon;		/* months since January [0, 11] */
	int tm_year;	/* years since 1900 */
	int tm_wday;	/* days since Sunday [0, 6] */
	int tm_yday;	/* days since January 1 [0, 365] */
	int tm_isdst;	/* Daylight Saving Time flag */
} dm, TIME;

clock_t clock(void);
double difftime(time_t time1, time_t time0);
time_t mktime(struct tm *pTime);
time_t time(time_t *pTime);
char *asctime(const struct tm *pTime);
char *ctime(const time_t *pTimer);
struct tm *gmtime(const time_t *pTimer);
size_t strftime(char *s, size_t max, const char *format, const struct tm *pTime);

#endif /* _TIME_H */

