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
 * aural_proto.h — the wire contract between the `aural` mixer server and its
 * clients (the libaudio veneer and native apps).  See docs/architecture/audio.md.
 *
 * Discipline (mirrors include/views/display_proto.h):
 *   - Control plane: fixed-size typed MPI messages (this header), validated at
 *     the boundary.  Carried in mpi_message_t.data; all fit MESSAGE_LENGTH (248).
 *   - Data plane: PCM only, in a shared-memory SPSC ring (struct aural_ring),
 *     allocated by aural and shared into the client via _sys_shareregion (the
 *     same server->client direction views uses for window buffers).  No PCM ever
 *     travels over MPI.
 */

#ifndef _AUDIO_AURAL_PROTO_H
#define _AUDIO_AURAL_PROTO_H

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** The mailbox every client posts control messages to. */
#define AURAL_MBOX "aural"

/** Protocol version — bumped on any incompatible struct change. */
#define AURAL_PROTO_VERSION 1u

/*
 * MPI message types (the mpi_message_t.header value).  Kept clear of the display
 * protocol's range to avoid confusion when reading logs; values are local to the
 * aural mailbox so there is no hard collision risk.
 */
#define AURAL_CLAIM 0x4110u      /* client -> aural : aural_claim_req  */
#define AURAL_ACK 0x4111u        /* aural -> client : aural_ack        */
#define AURAL_NAK 0x4112u        /* aural -> client : aural_nak        */
#define AURAL_START 0x4113u      /* client -> aural : aural_stream_ref */
#define AURAL_STOP 0x4114u       /* client -> aural : aural_stream_ref */
#define AURAL_SET_GAIN 0x4115u   /* client -> aural : aural_gain       */
#define AURAL_SET_MASTER 0x4116u /* client -> aural : aural_master     */
#define AURAL_RELEASE 0x4117u    /* client -> aural : aural_stream_ref */
#define AURAL_LIST 0x4118u       /* client -> aural : aural_list_req   */
#define AURAL_LIST_REPLY 0x4119u /* aural -> client : aural_list_reply */

/** PCM sample formats. v1 supports only interleaved signed 16-bit. */
#define AURAL_FMT_S16 1u

/** Max chars (incl. NUL) for a stream's display name carried in CLAIM/LIST. */
#define AURAL_NAME_MAX 24u

/* The v1 baseline the mixer runs at; clients are NAK'd to convert to this. */
#define AURAL_DEFAULT_RATE 48000u
#define AURAL_DEFAULT_CHANNELS 2u
#define AURAL_DEFAULT_FORMAT AURAL_FMT_S16

/** Per-stream gain is Q8 fixed point: 256 == unity, 0 == silent. */
#define AURAL_GAIN_UNITY 256u

	/* ------------------------------------------------------------------ */
	/* Control-plane DTOs (each lives in mpi_message_t.data, < 248 bytes). */
	/* ------------------------------------------------------------------ */

	/** AURAL_CLAIM — register a new playback stream. */
	struct aural_claim_req
	{
		uint32_t ver;              /* AURAL_PROTO_VERSION                          */
		int32_t sender_pid;        /* client pid — aural shares the ring into it   */
		uint32_t rate;             /* requested sample rate (Hz)                   */
		uint32_t channels;         /* requested channel count                      */
		uint32_t format;           /* AURAL_FMT_*                                  */
		char reply[32];            /* client's reply mailbox name                  */
		char name[AURAL_NAME_MAX]; /* app display name for the mixer flyout  */
	};

	/** AURAL_ACK — stream granted; carries the client-side ring address. */
	struct aural_ack
	{
		uint32_t ver;           /* AURAL_PROTO_VERSION                        */
		uint32_t stream_id;     /* opaque handle for later control messages   */
		uint64_t ring_vaddr;    /* struct aural_ring* in the CLIENT's space   */
		uint32_t ring_bytes;    /* total shared region size (header + data)   */
		uint32_t data_capacity; /* the ring's data[] capacity (power of two)  */
	};

	/** AURAL_NAK — claim refused; tells the client what format to convert to. */
	struct aural_nak
	{
		uint32_t ver;
		uint32_t reason; /* AURAL_NAK_* below                    */
		uint32_t supported_rate;
		uint32_t supported_channels;
		uint32_t supported_format;
	};

#define AURAL_NAK_FORMAT 1u  /* rate/channels/format not supported (yet) */
#define AURAL_NAK_NOSLOT 2u  /* no free stream slot                      */
#define AURAL_NAK_VERSION 3u /* protocol version mismatch               */

	/** AURAL_START / AURAL_STOP / AURAL_RELEASE — reference an existing stream. */
	struct aural_stream_ref
	{
		uint32_t stream_id;
	};

	/** AURAL_SET_GAIN — per-stream software gain (Q8, AURAL_GAIN_UNITY = 1.0). */
	struct aural_gain
	{
		uint32_t stream_id;
		uint32_t gain;
	};

/* AURAL_SET_MASTER field sentinel: "leave this control unchanged" so a
 * volume-only or mute-only update does not clobber the other. */
#define AURAL_MASTER_UNCHANGED 0xFFFFFFFFu

	/** AURAL_SET_MASTER — server-wide master volume (codec hardware control). */
	struct aural_master
	{
		uint32_t volume; /* 0..100, or AURAL_MASTER_UNCHANGED   */
		uint32_t mute;   /* 0 = unmute, 1 = mute, or UNCHANGED  */
	};

	/** AURAL_LIST — ask aural to enumerate active streams (for the mixer flyout). */
	struct aural_list_req
	{
		uint32_t ver;   /* AURAL_PROTO_VERSION          */
		char reply[32]; /* client's reply mailbox name  */
	};

/* Active streams that fit in one AURAL_LIST_REPLY (keeps it under MESSAGE_LENGTH).
 * More than this is truncated — six concurrent audio apps is ample for the MVP. */
#define AURAL_LIST_MAX 6u

	/** One row of AURAL_LIST_REPLY: a live stream the flyout draws a slider for. */
	struct aural_stream_info
	{
		uint32_t stream_id;        /* handle for AURAL_SET_GAIN            */
		uint32_t gain;             /* current Q8 gain (AURAL_GAIN_UNITY=1) */
		char name[AURAL_NAME_MAX]; /* app display name                    */
	};

	/** AURAL_LIST_REPLY — the current active-stream set (master comes from ubistry). */
	struct aural_list_reply
	{
		uint32_t ver;
		uint32_t count; /* valid entries in streams[] (<= AURAL_LIST_MAX) */
		struct aural_stream_info streams[AURAL_LIST_MAX];
	};

	/* ------------------------------------------------------------------ */
	/* Data-plane: the shared SPSC PCM ring (single producer = client,    */
	/* single consumer = aural).                                          */
	/* ------------------------------------------------------------------ */

	/*
	 * Lock-free single-producer/single-consumer byte ring.  The client advances
	 * write_idx after filling data[]; aural advances read_idx after draining it.
	 * Both indices are free-running uint32_t (natural wraparound); capacity is a
	 * power of two so the data offset is (idx & (capacity - 1)).
	 *
	 * Memory ordering: uBixOS is uniprocessor on both arches today, so a context
	 * switch between producer and consumer is a full barrier and `volatile` is
	 * sufficient.  When SMP lands this needs release (producer) / acquire (consumer)
	 * fences around the index updates.
	 */
	struct aural_ring
	{
		volatile uint32_t write_idx; /* bytes the producer has published */
		volatile uint32_t read_idx;  /* bytes the consumer has drained   */
		uint32_t capacity;           /* data[] size; MUST be a power of two */
		uint32_t frame_bytes;        /* channels * bytes/sample (info only) */
		uint8_t data[];              /* the PCM byte ring                   */
	};

	/**
	 * Initialise a freshly-allocated ring header.
	 *
	 * @param capacity  data[] size in bytes; must be a power of two.
	 */
	static inline void aural_ring_init(struct aural_ring *r, uint32_t capacity, uint32_t frame_bytes)
	{
		r->write_idx = 0;
		r->read_idx = 0;
		r->capacity = capacity;
		r->frame_bytes = frame_bytes;
	}

	/** @return bytes available to read (produced but not yet consumed). */
	static inline uint32_t aural_ring_readable(const struct aural_ring *r)
	{
		return r->write_idx - r->read_idx;
	}

	/** @return bytes of free space the producer may write. */
	static inline uint32_t aural_ring_writable(const struct aural_ring *r)
	{
		return r->capacity - (r->write_idx - r->read_idx);
	}

	/**
	 * Producer side: copy up to @p n bytes into the ring, return bytes accepted
	 * (may be short when the ring is nearly full).  Call from the client only.
	 */
	static inline uint32_t aural_ring_write(struct aural_ring *r, const void *src, uint32_t n)
	{
		uint32_t free_bytes = aural_ring_writable(r);
		uint32_t mask = r->capacity - 1u;
		uint32_t off, first;

		if (n > free_bytes)
			n = free_bytes;
		if (n == 0)
			return 0;

		off = r->write_idx & mask;
		first = r->capacity - off; /* bytes until the wrap point */
		if (first > n)
			first = n;

		memcpy(r->data + off, src, first);
		if (n > first)
			memcpy(r->data, (const uint8_t *)src + first, n - first);

		r->write_idx += n;
		return n;
	}

	/**
	 * Consumer side: copy up to @p n bytes out of the ring, return bytes read
	 * (may be short when the ring is nearly empty).  Call from aural only.
	 */
	static inline uint32_t aural_ring_read(struct aural_ring *r, void *dst, uint32_t n)
	{
		uint32_t avail = aural_ring_readable(r);
		uint32_t mask = r->capacity - 1u;
		uint32_t off, first;

		if (n > avail)
			n = avail;
		if (n == 0)
			return 0;

		off = r->read_idx & mask;
		first = r->capacity - off;
		if (first > n)
			first = n;

		memcpy(dst, r->data + off, first);
		if (n > first)
			memcpy((uint8_t *)dst + first, r->data, n - first);

		r->read_idx += n;
		return n;
	}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _AUDIO_AURAL_PROTO_H */
