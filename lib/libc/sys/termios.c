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

#include <termios.h>
#include <sys/ioccom.h>

int
tcgetattr(int fd, struct termios *t)
{
	return (ioctl(fd, TIOCGETA, t));
}

int
tcsetattr(int fd, int action, const struct termios *t)
{
	unsigned long req;

	switch (action) {
	case TCSADRAIN:	req = TIOCSETAW; break;
	case TCSAFLUSH:	req = TIOCSETAF; break;
	default:	req = TIOCSETA;  break;
	}
	return (ioctl(fd, req, (void *)t));
}

void
cfmakeraw(struct termios *t)
{
	t->c_iflag &= ~(IMAXBEL | IXOFF | IXON | ICRNL);
	t->c_oflag &= ~OPOST;
	t->c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHOKE | ECHONL | ECHOPRT |
	    ECHOCTL | ICANON | ISIG | IEXTEN);
	t->c_cc[VMIN]  = 1;
	t->c_cc[VTIME] = 0;
}

int
cfsetispeed(struct termios *t, speed_t speed)
{
	t->c_ispeed = speed;
	return (0);
}

int
cfsetospeed(struct termios *t, speed_t speed)
{
	t->c_ospeed = speed;
	return (0);
}

speed_t
cfgetispeed(const struct termios *t)
{
	return (t->c_ispeed);
}

speed_t
cfgetospeed(const struct termios *t)
{
	return (t->c_ospeed);
}
