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

#ifndef _TERMIOS_H
#define _TERMIOS_H

#include <sys/types.h>
#include <sys/ioccom.h>

#define NCCS	20

typedef unsigned int	tcflag_t;
typedef unsigned char	cc_t;
typedef unsigned int	speed_t;

struct termios {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t     c_cc[NCCS];
	speed_t  c_ispeed;
	speed_t  c_ospeed;
};

struct winsize {
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel;
	unsigned short ws_ypixel;
};

/* c_lflag bits */
#define ECHOKE		0x00000001
#define ECHOE		0x00000002
#define ECHOK		0x00000004
#define ECHO		0x00000008
#define ECHONL		0x00000010
#define ECHOPRT		0x00000020
#define ECHOCTL		0x00000040
#define ISIG		0x00000080
#define ICANON		0x00000100
#define IEXTEN		0x00000400

/* c_iflag bits */
#define ICRNL		0x00000100
#define IXON		0x00000200
#define IXOFF		0x00000400
#define IXANY		0x00000800
#define IMAXBEL		0x00002000

/* c_oflag bits */
#define OPOST		0x00000001

/* c_cc indices */
#define VEOF		0
#define VEOL		1
#define VEOL2		2
#define VERASE		3
#define VWERASE		4
#define VKILL		5
#define VREPRINT	6
#define VINTR		8
#define VQUIT		9
#define VSUSP		10
#define VDSUSP		11
#define VSTART		12
#define VSTOP		13
#define VLNEXT		14
#define VDISCARD	15
#define VMIN		16
#define VTIME		17
#define VSTATUS		18

/* tcsetattr action values */
#define TCSANOW		0
#define TCSADRAIN	1
#define TCSAFLUSH	2

/* ioctl request codes */
#define TIOCGETA	_IOR('t', 19, struct termios)
#define TIOCSETA	_IOW('t', 20, struct termios)
#define TIOCSETAW	_IOW('t', 21, struct termios)
#define TIOCSETAF	_IOW('t', 22, struct termios)
#define TIOCGWINSZ	_IOR('t', 104, struct winsize)
#define TIOCSWINSZ	_IOW('t', 103, struct winsize)

int	tcgetattr(int, struct termios *);
int	tcsetattr(int, int, const struct termios *);
void	cfmakeraw(struct termios *);
int	cfsetispeed(struct termios *, speed_t);
int	cfsetospeed(struct termios *, speed_t);
speed_t	cfgetispeed(const struct termios *);
speed_t	cfgetospeed(const struct termios *);

#endif
