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
 * random.c — kernel CSPRNG.  A ChaCha20 stream generator keyed from an entropy
 * pool that is stirred with RDTSC timing jitter at boot, on every timer tick,
 * and at each request.  Uses "fast key erasure": every output draw overwrites
 * the key with fresh keystream so previously-emitted bytes cannot be recomputed
 * from a later state capture (the construction behind OpenBSD arc4random and
 * the Linux getrandom() ChaCha path).
 *
 * Caveat: under QEMU/TCG, RDTSC jitter is far weaker than on bare metal (the
 * emulator can make back-to-back reads nearly deterministic).  This generator
 * is structurally sound, but its boot-time seed quality is only as good as the
 * platform's timing entropy.  A virtio-rng seed source can be layered in later
 * via krandom_stir() for a genuine hardware entropy input.
 */

#include <ubixos/random.h>
#include <ubixos/spinlock.h>
#include <ubixos/vitals.h>
#include <sys/types.h>
#include <machine/cpu.h> /* machine_cycles() — arch cycle counter */
#include <string.h>

#define CHACHA_ROTL(a, b) (((a) << (b)) | ((a) >> (32 - (b))))

#define CHACHA_QR(a, b, c, d)                                                                                          \
	do                                                                                                             \
	{                                                                                                              \
		(a) += (b);                                                                                            \
		(d) ^= (a);                                                                                            \
		(d) = CHACHA_ROTL((d), 16);                                                                            \
		(c) += (d);                                                                                            \
		(b) ^= (c);                                                                                            \
		(b) = CHACHA_ROTL((b), 12);                                                                            \
		(a) += (b);                                                                                            \
		(d) ^= (a);                                                                                            \
		(d) = CHACHA_ROTL((d), 8);                                                                             \
		(c) += (d);                                                                                            \
		(b) ^= (c);                                                                                            \
		(b) = CHACHA_ROTL((b), 7);                                                                             \
	} while (0)

/* Generator state.  g_key is the ChaCha key, replaced on every draw. */
static u_int8_t g_key[32];
static u_int64_t g_ctr;
static u_int8_t g_stir_idx;
static int g_inited;
static struct spinLock g_random_lock = SPIN_LOCK_INITIALIZER;

/**
 * Run the 20-round ChaCha block function: out = block(in), with the final
 * word-wise addition of the input state.
 */
static void chacha20_block(u_int32_t out[16], const u_int32_t in[16])
{
	u_int32_t x[16];
	int i;

	for (i = 0; i < 16; i++)
		x[i] = in[i];

	for (i = 0; i < 10; i++)
	{
		CHACHA_QR(x[0], x[4], x[8], x[12]);
		CHACHA_QR(x[1], x[5], x[9], x[13]);
		CHACHA_QR(x[2], x[6], x[10], x[14]);
		CHACHA_QR(x[3], x[7], x[11], x[15]);
		CHACHA_QR(x[0], x[5], x[10], x[15]);
		CHACHA_QR(x[1], x[6], x[11], x[12]);
		CHACHA_QR(x[2], x[7], x[8], x[13]);
		CHACHA_QR(x[3], x[4], x[9], x[14]);
	}

	for (i = 0; i < 16; i++)
		out[i] = x[i] + in[i];
}

/* see ubixos/random.h */
void krandom_stir(u_int64_t sample)
{
	u_int8_t idx;
	int i;

	/* Always fold in the cycle counter; the caller's sample is extra grist. */
	sample ^= machine_cycles();

	/*
	 * Lock-free XOR-fold into the key.  Racing with krandom_bytes() or a
	 * concurrent stir can at worst drop a sample, which only reduces the
	 * entropy added — never a correctness problem for a mixing pool.
	 */
	idx = g_stir_idx;
	for (i = 0; i < 8; i++)
		g_key[(idx + i) & 31] ^= (u_int8_t)(sample >> (i * 8));
	g_stir_idx = (u_int8_t)(idx + 8);
	g_ctr ^= sample;
}

/* see ubixos/random.h */
void krandom_init(void)
{
	volatile int onstack;
	int i;

	/* Mix in coarse boot state plus a burst of RDTSC samples (folded by stir). */
	krandom_stir((u_int64_t)(uintptr_t)&onstack);
	krandom_stir(systemVitals ? systemVitals->sysTicks : 0);
	for (i = 0; i < 256; i++)
		krandom_stir(0);

	g_inited = 1;
}

/* see ubixos/random.h */
void krandom_bytes(void *buf, size_t len)
{
	u_int8_t *out = (u_int8_t *)buf;

	/* Capture request timing as additional entropy. */
	krandom_stir(0);

	spinLock(&g_random_lock);

	/* If a caller beats krandom_init() (shouldn't happen), seed now. */
	if (!g_inited)
	{
		int i;
		for (i = 0; i < 64; i++)
			g_key[i & 31] ^= (u_int8_t)(machine_cycles() >> ((i & 7) * 8));
		g_inited = 1;
	}

	while (len > 0)
	{
		u_int32_t in[16], ks[16];
		size_t n;

		/* "expand 32-byte k" */
		in[0] = 0x61707865;
		in[1] = 0x3320646e;
		in[2] = 0x79622d32;
		in[3] = 0x6b206574;
		memcpy(&in[4], g_key, 32);
		in[12] = (u_int32_t)(g_ctr & 0xffffffff);
		in[13] = (u_int32_t)(g_ctr >> 32);
		in[14] = 0;
		in[15] = 0;
		g_ctr++;

		chacha20_block(ks, in);

		/* Fast key erasure: first 32 bytes become the next key. */
		memcpy(g_key, ks, 32);

		/* Remaining 32 bytes (ks[8..15]) are output. */
		n = (len < 32) ? len : 32;
		memcpy(out, (u_int8_t *)ks + 32, n);
		out += n;
		len -= n;
	}

	spinUnlock(&g_random_lock);
}
