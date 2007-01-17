/*****************************************************************************************
 Copyright (c) 2002-2004 The UbixOS Project
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are
 permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this list of
 conditions, the following disclaimer and the list of authors.  Redistributions in binary
 form must reproduce the above copyright notice, this list of conditions, the following
 disclaimer and the list of authors in the documentation and/or other materials provided
 with the distribution. Neither the name of the UbixOS Project nor the names of its
 contributors may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 $Id$

*****************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/sys.h>
#include <sys/sched.h>

void print2(char *string,int,int);

#define MINUTE 60
#define HOUR (60*MINUTE)
#define DAY (24*HOUR)
#define YEAR (365*DAY)
extern const char *__progname;


static int monthSecs[12] = {
        0,
        DAY*(31),
        DAY*(31+29),
        DAY*(31+29+31),
        DAY*(31+29+31+30),
        DAY*(31+29+31+30+31),
        DAY*(31+29+31+30+31+30),
        DAY*(31+29+31+30+31+30+31),
        DAY*(31+29+31+30+31+30+31+31),
        DAY*(31+29+31+30+31+30+31+31+30),
        DAY*(31+29+31+30+31+30+31+31+30+31),
        DAY*(31+29+31+30+31+30+31+31+30+31+30)
};

int main(int argc,char **argv) {
  int sysTime = 0x0;
  int i       = 0x0;
 
  int year  = 0x0;
  int month = 0x0;
  int day   = 0x0;
  int hour  = 0x0;
  int min   = 0x0;
  int sec   = 0x0;

  sysTime = gettime();

  year = (sysTime/YEAR) + 1970;
  sysTime -= (YEAR * (year-1970));
  sysTime -= DAY*(((year-1970)+1)/4);
  for (i = 11;i >= 0;i--) {
    if ((sysTime - monthSecs[i]) > 0) {
      month = i;
      break;
      }
    }
  sysTime -= monthSecs[i];
  if (((month > 1) && (((year-1970)+2)%4)) == 0x0) {
    sysTime += DAY;
    }
    
  day = (sysTime/DAY);
  sysTime -= (day*DAY);
  hour = (sysTime/HOUR);
  sysTime -= (hour*HOUR);
  min  = (sysTime/MINUTE);
  sysTime -= (min*MINUTE);
  sec = sysTime;

  printf("[%s][%02d/%02d/%i, %02d:%02d.%02d]\n",argv[0],month,day,year,hour,min,sec);
  for (i = 0x0;i < argc;i++) {
    printf("argv[%i](0x%X:%s),__progrname(%s), argc: %i\n",i,argv[i],argv[i],__progname,argc);
    }
  return(0);
  }

/***
 END
 ***/

