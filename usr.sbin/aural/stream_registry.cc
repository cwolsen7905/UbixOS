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

#include "stream_registry.hh"

#include <stdlib.h>
#include <string.h>
#include <ubix/process.hh> /* kill() — the views/vlogin precedent for liveness checks */
#include <sys/ubix_syscall.h>

#define PAGE_SIZE 4096u

namespace
{

/*
 * Native syscall 45: map a region from this process into another.  Same thunk
 * and direction (server -> client) the views compositor uses to hand a window
 * buffer to its clients.  The thunk does no marshalling — the kernel reads the
 * args from the platform C ABI locations.
 */
/* NOLINTBEGIN(readability-identifier-naming) — _sys_* is the fixed kernel ABI symbol. */
extern "C" int _sys_shareregion(int dst_pid, void *vaddr, uint32_t size, uintptr_t *out_vaddr);
UBIX_NATIVE_THUNK(_sys_shareregion, 45);
/* NOLINTEND(readability-identifier-naming) */

/** Round @p n up to a whole page. */
uint32_t page_round(uint32_t n)
{
	return (n + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
}

} /* anonymous namespace */

namespace aural
{

Stream *StreamRegistry::claim(int32_t pid,
                              uint32_t rate,
                              uint32_t channels,
                              uint32_t format,
                              uintptr_t &client_vaddr,
                              uint32_t &region_bytes,
                              uint32_t &capacity,
                              uint32_t &nak_reason)
{
	/* v1 accepts only the mixer's native format; the client converts otherwise. */
	if (rate != AURAL_DEFAULT_RATE || channels != AURAL_DEFAULT_CHANNELS || format != AURAL_DEFAULT_FORMAT)
	{
		nak_reason = AURAL_NAK_FORMAT;
		return nullptr;
	}

	Stream *slot = nullptr;
	for (unsigned i = 0; i < MAX_STREAMS; i++)
	{
		if (!slots_[i].in_use)
		{
			slot = &slots_[i];
			break;
		}
	}
	if (slot == nullptr)
	{
		nak_reason = AURAL_NAK_NOSLOT;
		return nullptr;
	}

	uint32_t total = page_round((uint32_t)sizeof(aural_ring) + RING_CAPACITY);
	void *region = aligned_alloc(PAGE_SIZE, total);
	if (region == nullptr)
	{
		nak_reason = AURAL_NAK_NOSLOT;
		return nullptr;
	}
	memset(region, 0, total);

	aural_ring *ring = (aural_ring *)region;
	aural_ring_init(ring, RING_CAPACITY, FRAME_BYTES);

	uintptr_t mapped = 0;
	if (_sys_shareregion(pid, region, total, &mapped) != 0 || mapped == 0)
	{
		free(region);
		nak_reason = AURAL_NAK_NOSLOT;
		return nullptr;
	}

	slot->in_use = true;
	slot->active = true; /* auto-start in v1 */
	slot->id = next_id_++;
	slot->client_pid = pid;
	slot->gain = AURAL_GAIN_UNITY;
	slot->region = region;
	slot->region_bytes = total;
	slot->ring = ring;

	client_vaddr = mapped;
	region_bytes = total;
	capacity = RING_CAPACITY;
	return slot;
}

bool StreamRegistry::release(uint32_t id)
{
	for (unsigned i = 0; i < MAX_STREAMS; i++)
	{
		if (slots_[i].in_use && slots_[i].id == id)
		{
			free(slots_[i].region);
			slots_[i] = Stream();
			return true;
		}
	}
	return false;
}

void StreamRegistry::reap()
{
	for (unsigned i = 0; i < MAX_STREAMS; i++)
	{
		if (slots_[i].in_use && ::kill(slots_[i].client_pid, 0) != 0)
		{
			free(slots_[i].region);
			slots_[i] = Stream();
		}
	}
}

Stream *StreamRegistry::find(uint32_t id)
{
	for (unsigned i = 0; i < MAX_STREAMS; i++)
	{
		if (slots_[i].in_use && slots_[i].id == id)
			return &slots_[i];
	}
	return nullptr;
}

} /* namespace aural */
