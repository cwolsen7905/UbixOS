/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ubixos/time.h>
#include <ubixos/vitals.h>
#include <lib/kprintf.h>
#include <assert.h>

static int month[12] = {0,
                        DAY * (31),
                        DAY * (31 + 29),
                        DAY * (31 + 29 + 31),
                        DAY * (31 + 29 + 31 + 30),
                        DAY * (31 + 29 + 31 + 30 + 31),
                        DAY * (31 + 29 + 31 + 30 + 31 + 30),
                        DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31),
                        DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31),
                        DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30),
                        DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31),
                        DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30)};

#if !defined(__aarch64__)
static int timeCmosRead(int addr)
{
	outportByteP(0x70, addr);
	return ((int)inportByte(0x71));
}

int time_init()
{
	int i;
	struct timeStruct time;

	for (i = 0; i < 1000000; i++)
	{
		if (!(timeCmosRead(10) & 0x80))
		{
			break;
		}
	}

	do
	{
		time.sec = timeCmosRead(0);
		time.min = timeCmosRead(2);
		time.hour = timeCmosRead(4);
		time.day = timeCmosRead(7);
		time.mon = timeCmosRead(8);
		time.year = timeCmosRead(9);
	} while (time.sec != timeCmosRead(0));

	BCD_TO_BIN(time.sec);
	BCD_TO_BIN(time.min);
	BCD_TO_BIN(time.hour);
	BCD_TO_BIN(time.day);
	BCD_TO_BIN(time.mon);
	BCD_TO_BIN(time.year);

	/* Set up our start time in seconds */
	systemVitals->timeStart = timeMake(&time);

	kprintf("%i/%i/%i %i:%i.%i\n", time.mon, time.day, time.year, time.hour, time.min, time.sec);

	/* Return so we know all went well */
	return (0x0);
}

/*
 * md_uptime (i386): monotonic time since boot from the PIT scheduler tick
 * (systemVitals->sysTicks at 200 Hz = 5 ms resolution).  i386 has no finer
 * free-running counter wired up, so the sub-second remainder is in 5 ms steps.
 */
void md_uptime(u_int64_t *sec, u_int64_t *nsec)
{
	u_int32_t ticks = (systemVitals != 0) ? systemVitals->sysTicks : 0;

	if (sec != 0)
		*sec = ticks / 200u;
	if (nsec != 0)
		*nsec = (u_int64_t)(ticks % 200u) * 5000000ULL; /* 5 ms in ns */
}
#endif /* !__aarch64__ */

u_int32_t timeMake(struct timeStruct *time)
{
	u_int32_t res;
	int year;

	year = (time->year + 100) - 70;

	/* magic offsets (y+1) needed to get leapyears right.*/
	res = YEAR * year + DAY * ((year + 1) / 4);

	res += month[time->mon];

	/* and (y+2) here. If it wasn't a leap-year, we have to adjust */
	if (time->mon > 1 && ((year + 2) % 4))
		res -= DAY;

	res += DAY * (time->day - 1);
	res += HOUR * time->hour;
	res += MINUTE * time->min;
	res += time->sec;

	return (res);
}

int gettimeofday(struct timeval *tp, struct timezone *tzp)
{
	u_int64_t sec, nsec;

	/*
	 * One way to get time: the wall clock = boot wall-clock second (timeStart,
	 * set per arch — i386 from the CMOS RTC in time_init, aarch64 epoch) plus the
	 * monotonic uptime from the single arch time source md_uptime (i386 PIT tick,
	 * aarch64 CNTVCT counter).  Without timeStart the clock reads ~1970, which
	 * breaks anything that needs real time — e.g. TLS cert-validity checks.
	 */
	md_uptime(&sec, &nsec);
	tp->tv_sec = systemVitals->timeStart + (u_int32_t)sec;
	tp->tv_usec = (suseconds_t)(nsec / 1000u);

	if (tzp != NULL)
	{
		tzp->tz_minuteswest = 0; /* UTC; no timezone database available */
		tzp->tz_dsttime = 0;
	}

	return (0);
}
