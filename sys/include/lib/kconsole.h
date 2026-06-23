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

#ifndef _KCONSOLE_H
#define _KCONSOLE_H

/*
 * Registered-sink kernel console (FreeBSD `consdev`-style).
 *
 * kprintf formats into a buffer (the shared kvprintf engine) and then hands the
 * string to kconsole_emit(), which walks every registered sink calling its
 * per-character putc.  Each arch registers what it physically has at boot — a
 * serial debug sink that is always on, plus (optionally) a primary visible
 * console that can be suspended while the graphical compositor owns the screen.
 *
 * Line-ending policy is deliberately left to each sink: a serial sink expands
 * '\n' to CR/LF, whereas a VGA text sink gives '\n' full-newline semantics
 * (column reset + line advance).  Centralising CR/LF the way stock FreeBSD does
 * would render a stray '\r' as a glyph on the VGA path, so it stays per-sink.
 */

#define KC_SERIAL 0x01u      /* always-on debug sink; never suspended */
#define KC_PRIMARY 0x02u     /* the primary visible console */
#define KC_SUSPENDABLE 0x04u /* may be suspended when the screen is claimed */

struct kconsole
{
	void (*putc)(int c);   /* emit one character to this device */
	const char *name;      /* short identifier, e.g. "com1", "vga", "pl011" */
	unsigned flags;        /* KC_* */
	struct kconsole *next; /* sink list link (managed by kconsole.c) */
};

void kconsole_register(struct kconsole *kc);
void kconsole_emit(const char *s);
/* Engage SMP console locking; call once the MMU is on and SMP is coming up (before
 * releasing the APs).  Before this, kconsole_emit emits unlocked (single-CPU + the
 * lock's atomics are unsafe pre-MMU). */
void kconsole_enable_mp(void);
void kconsole_suspend_primary(void);
void kconsole_resume_primary(void);

/*
 * Per-arch sink registration, implemented by each architecture and called once
 * from its early bootstrap before the first kprintf.  i386 registers COM1 + the
 * VGA primary console; aarch64 registers the PL011 serial sink.
 */
void kconsole_arch_init(void);

#endif /* _KCONSOLE_H */
