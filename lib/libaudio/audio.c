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

/*
 * libaudio — the application audio API.  This is the veneer over the `aural`
 * mixer server (docs/architecture/audio.md): the same device-style entry points
 * apps already use (audio_open/write/...) now connect to `aural` over MPI + a
 * shared PCM ring instead of opening /dev/audio directly, so every existing app
 * is mixed with no source change.
 *
 * Safety net: if `aural` is not running, every call falls back to the legacy
 * path (open /dev/audio + write/ioctl), so behaviour is identical to before the
 * mixer existed.  `aural` is the only process that may hold /dev/audio, so the
 * fallback only succeeds while the server is absent — which is exactly when it
 * should.
 */

#include <audio/audio.h>
#include <audio/aural_proto.h>
#include <sys/mpi.h>
#include <ubistry/ubistry.h>
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/*
 * audio_open returns an int "fd".  A legacy result is a real file descriptor
 * (small non-negative).  An aural stream is returned as (AURAL_HANDLE_FLAG |
 * slot) so the other entry points can tell the two apart and route accordingly.
 */
#define AURAL_HANDLE_FLAG 0x40000000
#define AURAL_MAX_STREAMS 8

/* Bounded spins so a wedged/dead server can never hang an app forever. */
#define AURAL_CLAIM_TRIES 200000
#define AURAL_WRITE_STALL 2000000

struct aural_client
{
	int in_use;
	uint32_t stream_id;
	struct aural_ring *ring; /* client-side mapping of the shared ring */
	char reply[32];          /* our private reply mailbox name */
};

static struct aural_client g_streams[AURAL_MAX_STREAMS];

/** @return non-zero if @p h is an aural-stream handle (not a legacy fd). */
static int is_aural(int h)
{
	return h >= 0 && (h & AURAL_HANDLE_FLAG) != 0;
}

/** @return the aural client for handle @p h, or NULL if out of range/closed. */
static struct aural_client *aural_of(int h)
{
	int idx = h & ~AURAL_HANDLE_FLAG;
	if (idx < 0 || idx >= AURAL_MAX_STREAMS || !g_streams[idx].in_use)
		return NULL;
	return &g_streams[idx];
}

/**
 * Try to register a stream with the aural server.
 *
 * @return the aural handle on success, or -1 if aural is absent / refused
 *         (the caller then falls back to the legacy device path).
 */
static int aural_try_open(void)
{
	int slot = -1;
	for (int i = 0; i < AURAL_MAX_STREAMS; i++)
	{
		if (!g_streams[i].in_use)
		{
			slot = i;
			break;
		}
	}
	if (slot < 0)
		return -1;

	char name[32];
	snprintf(name, sizeof(name), "auralc.%d.%d", (int)getpid(), slot);
	if (mpi_createMbox(name) != 0)
		return -1;

	mpi_message_t msg;
	memset(&msg, 0, sizeof(msg));
	struct aural_claim_req *req = (struct aural_claim_req *)msg.data;
	req->ver = AURAL_PROTO_VERSION;
	req->sender_pid = (int32_t)getpid();
	req->rate = AURAL_DEFAULT_RATE;
	req->channels = AURAL_DEFAULT_CHANNELS;
	req->format = AURAL_DEFAULT_FORMAT;
	strncpy(req->reply, name, sizeof(req->reply) - 1);
	/* Display name for the mixer flyout: the program's basename (musl sets this
	 * from argv[0]); fall back to "audio" for the rare nameless process. */
	extern char *program_invocation_short_name;
	const char *appname = program_invocation_short_name;
	strncpy(req->name, (appname != NULL && appname[0] != '\0') ? appname : "audio", sizeof(req->name) - 1);
	msg.header = AURAL_CLAIM; /* MPI delivers msg.header, NOT the type arg */

	/* mpi_postMessage returns non-zero when the "aural" mailbox does not exist
	 * — i.e. the server is not running.  Fall back to the legacy device. */
	if (mpi_postMessage(AURAL_MBOX, AURAL_CLAIM, &msg) != 0)
	{
		mpi_destroyMbox(name);
		return -1;
	}

	for (int tries = 0; tries < AURAL_CLAIM_TRIES; tries++)
	{
		mpi_message_t rep;
		if (mpi_fetchMessage(name, &rep) == 0)
		{
			if (rep.header == AURAL_ACK)
			{
				struct aural_ack *ack = (struct aural_ack *)rep.data;
				g_streams[slot].in_use = 1;
				g_streams[slot].stream_id = ack->stream_id;
				g_streams[slot].ring = (struct aural_ring *)(uintptr_t)ack->ring_vaddr;
				strncpy(g_streams[slot].reply, name, sizeof(g_streams[slot].reply) - 1);
				return AURAL_HANDLE_FLAG | slot;
			}
			/* NAK (bad format / no slot) — give up on the server path. */
			mpi_destroyMbox(name);
			return -1;
		}
		sched_yield();
	}

	mpi_destroyMbox(name);
	return -1;
}

int audio_open(const char *dev)
{
	int h = aural_try_open();
	if (h >= 0)
		return h;
	return open(dev, O_WRONLY); /* legacy: aural absent */
}

int audio_set_rate(int fd, uint32_t rate)
{
	if (is_aural(fd))
	{
		/* aural passes client PCM straight through to the codec (no resample),
		 * so the device's native rate governs playback and the requested rate is
		 * advisory — accept it rather than failing a 44.1 kHz caller like vDoom. */
		(void)rate;
		return 0;
	}
	return ioctl(fd, AUDIO_SET_RATE, &rate);
}

/** Send an AURAL_SET_MASTER with one field left unchanged. */
static int aural_set_master(uint32_t volume, uint32_t mute)
{
	mpi_message_t msg;
	memset(&msg, 0, sizeof(msg));
	struct aural_master *m = (struct aural_master *)msg.data;
	m->volume = volume;
	m->mute = mute;
	msg.header = AURAL_SET_MASTER; /* MPI delivers msg.header, NOT the type arg */
	return mpi_postMessage(AURAL_MBOX, AURAL_SET_MASTER, &msg) == 0 ? 0 : -1;
}

int audio_set_volume(int fd, uint32_t pct)
{
	if (is_aural(fd))
		return aural_set_master(pct, AURAL_MASTER_UNCHANGED);
	return ioctl(fd, AUDIO_SET_VOLUME, &pct);
}

int audio_get_volume(int fd, uint32_t *pct)
{
	if (is_aural(fd))
	{
		int v = 100;
		(void)ubistry_get_int("/aural/volume", &v); /* aural owns master in ubistry */
		*pct = (uint32_t)v;
		return 0;
	}
	return ioctl(fd, AUDIO_GET_VOLUME, pct);
}

int audio_set_mute(int fd, uint32_t mute)
{
	if (is_aural(fd))
		return aural_set_master(AURAL_MASTER_UNCHANGED, mute ? 1u : 0u);
	return ioctl(fd, AUDIO_SET_MUTE, &mute);
}

int audio_get_mute(int fd, uint32_t *mute)
{
	if (is_aural(fd))
	{
		int m = 0;
		(void)ubistry_get_int("/aural/mute", &m);
		*mute = m ? 1u : 0u;
		return 0;
	}
	return ioctl(fd, AUDIO_GET_MUTE, mute);
}

int audio_write(int fd, const void *buf, int n)
{
	const char *p = (const char *)buf;
	int total = 0;

	if (is_aural(fd))
	{
		struct aural_client *c = aural_of(fd);
		long stall = 0;

		if (c == NULL)
			return -1;

		while (total < n)
		{
			uint32_t w = aural_ring_write(c->ring, p + total, (uint32_t)(n - total));
			if (w > 0)
			{
				total += (int)w;
				stall = 0;
			}
			else if (++stall > AURAL_WRITE_STALL)
			{
				/* server not draining (gone/stuck) — return a short write. */
				return total;
			}
			else
			{
				sched_yield(); /* ring full — let aural drain, then retry */
			}
		}
		return total;
	}

	/* Legacy path: the kernel write is non-blocking (0 = ring full); retry in
	 * user mode (IF=1) so the device ISR can drain between attempts. */
	while (total < n)
	{
		int r = (int)write(fd, p + total, (size_t)(n - total));
		if (r < 0)
			return -1;
		if (r > 0)
			total += r;
		else
			sched_yield();
	}
	return total;
}

void audio_close(int fd)
{
	if (is_aural(fd))
	{
		struct aural_client *c = aural_of(fd);
		if (c != NULL)
		{
			mpi_message_t msg;
			memset(&msg, 0, sizeof(msg));
			struct aural_stream_ref *ref = (struct aural_stream_ref *)msg.data;
			ref->stream_id = c->stream_id;
			msg.header = AURAL_RELEASE; /* MPI delivers msg.header, NOT the type arg */
			mpi_postMessage(AURAL_MBOX, AURAL_RELEASE, &msg);
			mpi_destroyMbox(c->reply);
			c->in_use = 0;
			c->ring = NULL;
		}
		return;
	}
	close(fd);
}
