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

#include "aural_server.hh"

#include <string.h>
#include <time.h>
#include <sys/mpi.h>
#include <api/ubix.h>

/* musl gates the nanosleep prototype behind a feature macro the -nostdinc world
 * build does not set; the symbol is in libc, so declare it directly. */
extern "C" int nanosleep(const struct timespec *req, struct timespec *rem);

namespace
{

/** Sleep ~AURAL_NAP_MS (descheduled) — the mixer's pacing tick; never spins. */
void aural_nap(void)
{
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = (long)aural::AURAL_NAP_MS * 1000 * 1000;
	nanosleep(&ts, 0);
}

/** Post a typed reply DTO to a client's reply mailbox. */
void post_reply(const char *mbox, uint32_t header, const void *payload, unsigned len)
{
	mpi_message_t msg;
	memset(&msg, 0, sizeof(msg));
	msg.header = header;
	if (len > MESSAGE_LENGTH)
		len = MESSAGE_LENGTH;
	memcpy(msg.data, payload, len);
	mpi_postMessage(mbox, header, &msg);
}

} /* anonymous namespace */

namespace aural
{

int AuralServer::run()
{
	mbox_.assign(AURAL_MBOX);
	if (!mbox_.create())
	{
		ulog(ULOG_ERR, "aural: cannot create mailbox");
		return 1;
	}

	/* /dev/audio may not be open-able the instant init forks us at boot; retry
	 * briefly (descheduled naps) before giving up. */
	int tries;
	for (tries = 0; tries < 100; tries++)
	{
		if (sink_.open_device())
			break;
		aural_nap();
	}
	if (tries >= 100)
	{
		ulog(ULOG_INFO, "aural: no /dev/audio — exiting");
		return 1;
	}
	ulog(ULOG_INFO, "aural: mixer up, owning /dev/audio");

	for (;;)
	{
		mpi_message_t msg;
		while (mbox_.try_fetch(msg))
			dispatch(msg.header, msg.data);

		/*
		 * Priming pass (once per wake): a stream starts being mixed once its ring
		 * has built a cushion (so a continuous stream's device buffer fills and
		 * never underruns), OR after a short grace period (so a brief clip that
		 * never reaches the cushion still plays).
		 */
		{
			Stream *sl = reg_.slots();
			for (unsigned k = 0; k < StreamRegistry::slot_count(); k++)
			{
				if (!sl[k].in_use || !sl[k].active || sl[k].ring == 0 || sl[k].primed)
					continue;
				uint32_t r = aural_ring_readable(sl[k].ring);
				if (r == 0)
				{
					sl[k].prime_wait = 0;
					continue;
				}
				if (r >= (uint32_t)PRIME_BYTES || ++sl[k].prime_wait >= (uint32_t)PRIME_GRACE)
					sl[k].primed = true;
			}
		}

		/*
		 * Feed pass: submit variable-length chunks of REAL audio (never silence-
		 * pad between a client's bursts — that is what makes the output choppy).
		 * Each chunk is as large as the busiest primed stream has, up to one
		 * period; quieter streams contribute what they have.
		 */
		for (int i = 0; i < AURAL_FEED_BURST; i++)
		{
			uint32_t ready = 0; /* max readable among PRIMED streams */
			Stream *sl = reg_.slots();
			for (unsigned k = 0; k < StreamRegistry::slot_count(); k++)
			{
				if (sl[k].in_use && sl[k].active && sl[k].primed && sl[k].ring != 0)
				{
					uint32_t r = aural_ring_readable(sl[k].ring);
					if (r > ready)
						ready = r;
				}
			}
			if (ready == 0)
				break; /* nothing to play */

			uint32_t chunk = ready < (uint32_t)PERIOD_BYTES ? ready : (uint32_t)PERIOD_BYTES;
			unsigned frames = chunk / FRAME_BYTES;
			if (frames == 0)
				break;

			mixer_.mix(reg_, period_, frames);
			if (sink_.write_period(period_, frames * FRAME_BYTES) <= 0)
				break; /* driver ring full */
		}

		aural_nap(); /* real ~10 ms descheduling sleep — never busy-spins */

		if (++reap_ctr_ >= REAP_INTERVAL)
		{
			reg_.reap();
			reap_ctr_ = 0;
		}
	}
}

void AuralServer::dispatch(uint32_t header, void *data)
{
	switch (header)
	{
		case AURAL_CLAIM:
			handle_claim((struct aural_claim_req *)data);
			break;
		case AURAL_RELEASE:
			handle_release((struct aural_stream_ref *)data);
			break;
		case AURAL_START:
			handle_active((struct aural_stream_ref *)data, true);
			break;
		case AURAL_STOP:
			handle_active((struct aural_stream_ref *)data, false);
			break;
		case AURAL_SET_GAIN:
			handle_gain((struct aural_gain *)data);
			break;
		case AURAL_SET_MASTER:
			handle_master((struct aural_master *)data);
			break;
		case AURAL_LIST:
			handle_list((struct aural_list_req *)data);
			break;
		default:
			break; /* unknown control message — ignore */
	}
}

void AuralServer::handle_claim(struct aural_claim_req *req)
{
	if (req->ver != AURAL_PROTO_VERSION)
	{
		struct aural_nak nak;
		memset(&nak, 0, sizeof(nak));
		nak.ver = AURAL_PROTO_VERSION;
		nak.reason = AURAL_NAK_VERSION;
		post_reply(req->reply, AURAL_NAK, &nak, sizeof(nak));
		return;
	}

	uintptr_t client_vaddr = 0;
	uint32_t region_bytes = 0;
	uint32_t capacity = 0;
	uint32_t reason = 0;

	Stream *s = reg_.claim(
	    req->sender_pid, req->rate, req->channels, req->format, client_vaddr, region_bytes, capacity, reason);
	if (s == nullptr)
	{
		struct aural_nak nak;
		memset(&nak, 0, sizeof(nak));
		nak.ver = AURAL_PROTO_VERSION;
		nak.reason = reason;
		nak.supported_rate = AURAL_DEFAULT_RATE;
		nak.supported_channels = AURAL_DEFAULT_CHANNELS;
		nak.supported_format = AURAL_DEFAULT_FORMAT;
		post_reply(req->reply, AURAL_NAK, &nak, sizeof(nak));
		return;
	}

	strncpy(s->name, req->name, sizeof(s->name) - 1);
	s->name[sizeof(s->name) - 1] = '\0';

	struct aural_ack ack;
	memset(&ack, 0, sizeof(ack));
	ack.ver = AURAL_PROTO_VERSION;
	ack.stream_id = s->id;
	ack.ring_vaddr = (uint64_t)client_vaddr;
	ack.ring_bytes = region_bytes;
	ack.data_capacity = capacity;
	post_reply(req->reply, AURAL_ACK, &ack, sizeof(ack));
}

void AuralServer::handle_release(struct aural_stream_ref *ref)
{
	reg_.release(ref->stream_id);
}

void AuralServer::handle_active(struct aural_stream_ref *ref, bool active)
{
	Stream *s = reg_.find(ref->stream_id);
	if (s != nullptr)
		s->active = active;
}

void AuralServer::handle_gain(struct aural_gain *g)
{
	Stream *s = reg_.find(g->stream_id);
	if (s == nullptr)
		return;

	uint32_t gain = g->gain;
	if (gain > 4u * AURAL_GAIN_UNITY) /* cap at +12 dB to bound mix overflow */
		gain = 4u * AURAL_GAIN_UNITY;
	s->gain = gain;
}

void AuralServer::handle_master(struct aural_master *m)
{
	sink_.set_master(m->volume, m->mute, true);
}

/**
 * AURAL_LIST — enumerate the live streams (one slider per claimed stream) for
 * the taskbar mixer flyout.  Master volume is not included here; the flyout
 * reads it from ubistry (/aural/volume), the same source the codec is seeded
 * from.  Truncates at AURAL_LIST_MAX (six concurrent apps is ample for v1).
 */
void AuralServer::handle_list(struct aural_list_req *req)
{
	struct aural_list_reply rep;
	memset(&rep, 0, sizeof(rep));
	rep.ver = AURAL_PROTO_VERSION;

	Stream *sl = reg_.slots();
	for (unsigned k = 0; k < StreamRegistry::slot_count() && rep.count < AURAL_LIST_MAX; k++)
	{
		if (!sl[k].in_use)
			continue;
		struct aural_stream_info *info = &rep.streams[rep.count++];
		info->stream_id = sl[k].id;
		info->gain = sl[k].gain;
		strncpy(info->name, sl[k].name, sizeof(info->name) - 1);
	}

	post_reply(req->reply, AURAL_LIST_REPLY, &rep, sizeof(rep));
}

} /* namespace aural */
