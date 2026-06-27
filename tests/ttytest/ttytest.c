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

/*
 * ttytest — Phase 1 & 2 verification for ioctl/termios.
 *
 * Tests (in order):
 *   1. tcgetattr: reads live flags, prints c_lflag in hex
 *   2. TIOCGWINSZ: prints rows x cols (expect 25x80)
 *   3. isatty: prints result for fd 0 (expect 1)
 *   4. cfmakeraw + tcsetattr: enters raw mode, reads one char without Enter
 *   5. tcsetattr restore: returns to canonical mode, verifies with tcgetattr
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

int
main(void)
{
	struct termios t, raw;
	struct winsize ws;
	char ch;
	int ret;

	/* 1. tcgetattr */
	ret = tcgetattr(0, &t);
	printf("tcgetattr(0): ret=%d  c_lflag=0x%x\n", ret, t.c_lflag);
	if (ret != 0) {
		printf("FAIL: tcgetattr returned %d\n", ret);
		return (1);
	}
	if (t.c_lflag & ICANON)
		printf("  ICANON set (canonical mode) - OK\n");
	else
		printf("  ICANON clear (raw mode) - unexpected at startup\n");
	if (t.c_lflag & ECHO)
		printf("  ECHO set - OK\n");
	else
		printf("  ECHO clear - unexpected at startup\n");

	/* 2. TIOCGWINSZ */
	ret = ioctl(0, TIOCGWINSZ, &ws);
	printf("TIOCGWINSZ: ret=%d  rows=%d cols=%d\n",
	    ret, ws.ws_row, ws.ws_col);
	if (ws.ws_row == 25 && ws.ws_col == 80)
		printf("  window size OK\n");
	else
		printf("  FAIL: expected 25x80\n");

	/* 3. isatty */
	printf("isatty(0)=%d  (expect 1)\n", isatty(0));

	/* 4. raw mode: read one char without Enter */
	raw = t;
	cfmakeraw(&raw);
	ret = tcsetattr(0, TCSANOW, &raw);
	printf("tcsetattr raw: ret=%d\n", ret);

	printf("Press any key (no Enter needed): ");
	ret = read(0, &ch, 1);
	printf("\nGot char: 0x%02x ('%c')  read_ret=%d\n",
	    (unsigned char)ch,
	    (ch >= 0x20 && ch < 0x7f) ? ch : '.',
	    ret);

	/* verify ICANON is clear after tcsetattr */
	tcgetattr(0, &raw);
	if (raw.c_lflag & ICANON)
		printf("FAIL: ICANON still set after cfmakeraw\n");
	else
		printf("ICANON clear after cfmakeraw - OK\n");

	/* 5. restore canonical */
	ret = tcsetattr(0, TCSANOW, &t);
	printf("tcsetattr restore: ret=%d\n", ret);
	tcgetattr(0, &raw);
	if (raw.c_lflag & ICANON)
		printf("ICANON restored - OK\n");
	else
		printf("FAIL: ICANON not restored\n");

	printf("ttytest complete.\n");
	return (0);
}
