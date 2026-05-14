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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MINUTE 60
#define HOUR (60*MINUTE)
#define DAY (24*HOUR)
#define YEAR (365*DAY)

static int monthSecs[12] = { 0,
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
DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30) };

int main(int argc, char **argv) {
  struct timespec ts;
  int sysTime = 0;
  int i = 0;

  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int min = 0;
  int sec = 0;

  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
    fprintf(stderr, "clock: clock_gettime failed\n");
    return 1;
  }
  sysTime = (int)ts.tv_sec;

  year = (sysTime / YEAR) + 1970;
  sysTime -= (YEAR * (year - 1970));
  sysTime -= DAY * (((year - 1970) + 1) / 4);
  for (i = 11; i >= 0; i--) {
    if ((sysTime - monthSecs[i]) > 0) {
      month = i;
      break;
    }
  }
  sysTime -= monthSecs[i];
  if (((month > 1) && (((year - 1970) + 2) % 4)) == 0) {
    sysTime += DAY;
  }

  day = (sysTime / DAY);
  sysTime -= (day * DAY);
  hour = (sysTime / HOUR);
  sysTime -= (hour * HOUR);
  min = (sysTime / MINUTE);
  sysTime -= (min * MINUTE);
  sec = sysTime;

  printf("[%s][%02d/%02d/%i, %02d:%02d.%02d]\n",
         argv[0], month, day, year, hour, min, sec);
  return 0;
}
