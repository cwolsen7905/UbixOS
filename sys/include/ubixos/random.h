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

#ifndef _UBIXOS_RANDOM_H
#define _UBIXOS_RANDOM_H

#include <sys/types.h>

/**
 * Seed the kernel CSPRNG at boot from whatever jitter is available.
 * Must be called once, after systemVitals is live, before userland starts.
 */
void krandom_init(void);

/**
 * Mix a timing/jitter sample into the entropy pool.  Lock-free and cheap;
 * safe to call from interrupt context (e.g. the timer tick or keyboard IRQ).
 * Losing a concurrent sample to a race is harmless — it only adds entropy.
 */
void krandom_stir(u_int64_t sample);

/**
 * Fill @p buf with @p len cryptographically-random bytes from the ChaCha20
 * generator (fast-key-erasure: each call rekeys, giving backtracking
 * resistance).  Backs /dev/random, /dev/urandom and sys_getrandom.
 */
void krandom_bytes(void *buf, size_t len);

#endif /* _UBIXOS_RANDOM_H */
