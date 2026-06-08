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
 *
 * pty.c — userland thunks for the pseudo-terminal pool syscalls (int $0x81,
 * native slots 55-58).  A graphical terminal app allocates a slot, runs an
 * interactive shell against /dev/ttyv<slot>, feeds keystrokes with pty_inject(),
 * and reads the rendered 80x25 cell grid back with pty_snapshot().  Modeled on
 * settty.c.
 */

#include <sys/ubix_syscall.h>
UBIX_NATIVE_THUNK(_do_ptyalloc, 55);
UBIX_NATIVE_THUNK(_do_ptyfree, 56);
UBIX_NATIVE_THUNK(_do_ptyinject, 57);
UBIX_NATIVE_THUNK(_do_ptysnap, 58);
UBIX_NATIVE_THUNK(_do_ptyresize, 61);

static int _do_ptyalloc(void);
static int _do_ptyfree(int slot);
static int _do_ptyinject(int slot, const char *buf, int n);
static int _do_ptysnap(int slot, void *dst, unsigned short *x, unsigned short *y);
static int _do_ptyresize(int slot, int cols, int rows);

/**
 * Allocate a pseudo-terminal slot from the kernel pty pool.
 * @return slot index (open /dev/ttyv<slot>), or -1 if the pool is full.
 */
int pty_alloc(void)
{
	return _do_ptyalloc();
}

/**
 * Release a pseudo-terminal slot back to the pool.
 */
int pty_free(int slot)
{
	return _do_ptyfree(slot);
}

/**
 * Feed a keystroke buffer through the pty's line discipline (→ shell stdin).
 * @return bytes injected, or -1 for an invalid slot.
 */
int pty_inject(int slot, const char *buf, int n)
{
	return _do_ptyinject(slot, buf, n);
}

/**
 * Copy the pty's rendered 80x25 cell grid (80*25*2 bytes of char+attribute)
 * and cursor position out for drawing.
 * @return 0 on success, -1 for an invalid slot.
 */
int pty_snapshot(int slot, void *dst, unsigned short *x, unsigned short *y)
{
	return _do_ptysnap(slot, dst, x, y);
}

/**
 * Resize a pty's cell grid to cols x rows (clamped by the kernel) and update its
 * winsize.  @return 0 on success, -1 for an invalid slot.
 */
int pty_resize(int slot, int cols, int rows)
{
	return _do_ptyresize(slot, cols, rows);
}
