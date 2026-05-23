/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

/*
 * Ioctl's have the command encoded in the lower word, and the size of
 * any in or out parameters in the upper word.  The high 3 bits of the
 * upper word are used to encode the in/out status of the parameter.
 */
#define IOCPARM_SHIFT   13              /* number of bits for ioctl size */
#define IOCPARM_MASK    ((1 << IOCPARM_SHIFT) - 1) /* parameter length mask */
#define IOCPARM_LEN(x)  (((x) >> 16) & IOCPARM_MASK)
#define IOCBASECMD(x)   ((x) & ~(IOCPARM_MASK << 16))
#define IOCGROUP(x)     (((x) >> 8) & 0xff)

#define IOCPARM_MAX     (1 << IOCPARM_SHIFT) /* max size of ioctl */
#define IOC_VOID        0x20000000      /* no parameters */
#define IOC_OUT         0x40000000      /* copy out parameters */
#define IOC_IN          0x80000000      /* copy in parameters */
#define IOC_INOUT       (IOC_IN|IOC_OUT)
#define IOC_DIRMASK     (IOC_VOID|IOC_OUT|IOC_IN)

#define _IOC(inout,group,num,len)       ((unsigned long) \
        ((inout) | (((len) & IOCPARM_MASK) << 16) | ((group) << 8) | (num)))
#define _IO(g,n)        _IOC(IOC_VOID,  (g), (n), 0)
#define _IOWINT(g,n)    _IOC(IOC_VOID,  (g), (n), sizeof(int))
#define _IOR(g,n,t)     _IOC(IOC_OUT,   (g), (n), sizeof(t))
#define _IOW(g,n,t)     _IOC(IOC_IN,    (g), (n), sizeof(t))
/* this should be _IORW, but stdio got there first */
#define _IOWR(g,n,t)    _IOC(IOC_INOUT, (g), (n), sizeof(t))

#define IOCPARM_IVAL(x) ((int)(intptr_t)(void *)*(caddr_t *)(void *)(x))

#define NCCS 20

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

struct termios {
    tcflag_t c_iflag; /* input flags */
    tcflag_t c_oflag; /* output flags */
    tcflag_t c_cflag; /* control flags */
    tcflag_t c_lflag; /* local flags */
    cc_t c_cc[NCCS]; /* control chars */
    speed_t c_ispeed; /* input speed */
    speed_t c_ospeed; /* output speed */
};

struct winsize {
    unsigned short ws_row; /* rows, in characters */
    unsigned short ws_col; /* columns, in characters */
    unsigned short ws_xpixel; /* horizontal size, pixels */
    unsigned short ws_ypixel; /* vertical size, pixels */
};

#define TIOCGETA        _IOR('t', 19, struct termios)  /* get termios struct */
#define TIOCSETA        _IOW('t', 20, struct termios)  /* set termios struct */
#define TIOCSETAW       _IOW('t', 21, struct termios)  /* drain, then set */
#define TIOCSETAF       _IOW('t', 22, struct termios)  /* flush, then set */
#define TIOCGWINSZ      _IOR('t', 104, struct winsize) /* get window size */
#define TIOCSWINSZ      _IOW('t', 103, struct winsize) /* set window size */

/* c_lflag bits */
#define ECHOKE          0x00000001
#define ECHOE           0x00000002
#define ECHOK           0x00000004
#define ECHO            0x00000008
#define ECHONL          0x00000010
#define ECHOPRT         0x00000020
#define ECHOCTL         0x00000040
#define ISIG            0x00000080
#define ICANON          0x00000100
#define IEXTEN          0x00000400

#define TIOCGPGRP       _IOR('t', 119, int)    /* get foreground pgrp */
#define TIOCSPGRP       _IOW('t', 118, int)    /* set foreground pgrp */
#define TIOCSCTTY       _IO('t',  97)          /* become controlling terminal */
#define FIONREAD        _IOR('f', 127, int)    /* get # bytes to read */

#endif /* _SYS_IOCTL_H */
