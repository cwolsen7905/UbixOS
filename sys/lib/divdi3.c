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

#include <lib/libkern.h>

u_quad_t
__udivdi3(u_quad_t a, u_quad_t b)
{
	return (__qdivrem(a, b, (u_quad_t *)0));
}

quad_t
__divdi3(quad_t a, quad_t b)
{
	u_quad_t ua, ub, uq;
	int neg;

	if (a < 0) {
		ua = -(u_quad_t)a;
		neg = 1;
	} else {
		ua = (u_quad_t)a;
		neg = 0;
	}
	if (b < 0) {
		ub = -(u_quad_t)b;
		neg ^= 1;
	} else {
		ub = (u_quad_t)b;
	}
	uq = __qdivrem(ua, ub, (u_quad_t *)0);
	return (neg ? -(quad_t)uq : (quad_t)uq);
}

quad_t
__moddi3(quad_t a, quad_t b)
{
	u_quad_t ua, ub, ur;
	int neg;

	if (a < 0) {
		ua = -(u_quad_t)a;
		neg = 1;
	} else {
		ua = (u_quad_t)a;
		neg = 0;
	}
	ub = (b < 0) ? -(u_quad_t)b : (u_quad_t)b;
	(void)__qdivrem(ua, ub, &ur);
	return (neg ? -(quad_t)ur : (quad_t)ur);
}
