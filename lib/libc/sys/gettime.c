/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
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

#include <sys/types.h>
#include <sys/time.h>

/*
 * Raw syscall stub: args already on stack from the C call, just set %eax
 * to syscall 116 (gettimeofday) and fire.
 */
asm(".globl _sys_gettimeofday\n"
    "_sys_gettimeofday:\n"
    "  movl $116,%eax\n"
    "  int $0x80\n"
    "  ret\n");

extern int _sys_gettimeofday(struct timeval *tp, struct timezone *tzp);

/*
 * gettime() — return current Unix timestamp (seconds since epoch).
 *
 * Previously called syscall 47 (getgid) by mistake, always returning 0.
 * Now calls gettimeofday (116) which returns timeStart + sysUptime,
 * where timeStart was read from the CMOS RTC at boot.
 */
int gettime(void) {
    struct timeval tv;
    struct timezone tz;

    tv.tv_sec  = 0;
    tv.tv_usec = 0;
    tz.tz_minuteswest = 0;
    tz.tz_dsttime     = 0;

    _sys_gettimeofday(&tv, &tz);

    return ((int) tv.tv_sec);
}
