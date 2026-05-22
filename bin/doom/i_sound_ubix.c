//
// Copyright (c) 2002-2026 The UbixOS Project.
//
// UbixOS AC97 sound backend for doomgeneric.
// Distributed under the GNU General Public License v2 (same as doomgeneric).
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
//

/*
 * i_sound_ubix.c — DOOM digital sound effects for UbixOS.
 *
 * Implements the I_* sound interface used by doomgeneric.  Music is handled
 * separately by i_music_ubix.c (stubbed out for now).
 *
 * Design:
 *   - Up to NUM_CHANNELS simultaneous sound effects.
 *   - WAD sound lumps are unsigned 8-bit PCM, typically 11025 Hz.
 *   - We output signed 16-bit stereo at MIX_RATE Hz via /dev/audio (AC97).
 *   - I_UpdateSound() is called once per 35 Hz game tick; it mixes
 *     MIX_RATE/35 samples and writes them to the audio ring buffer.
 *   - Upsampling is nearest-neighbour (fixed-point step accumulator).
 */

#include "i_sound.h"
#include "sounds.h"
#include "w_wad.h"
#include "z_zone.h"

#include <audio/audio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define NUM_CHANNELS  8
#define MIX_RATE      44100

/* Samples per game tick (35 Hz).  Add 1 to handle rounding. */
#define TICK_SAMPLES  ((MIX_RATE / 35) + 1)

/* WAD sound lump header (little-endian) */
typedef struct {
	uint16_t format;   /* 0x0003 = raw PCM */
	uint16_t rate;     /* source sample rate */
	uint32_t count;    /* number of samples */
} snd_hdr_t;

typedef struct {
	const uint8_t *data;    /* raw u8 PCM (points inside WAD cache) */
	int32_t        len;     /* sample count */
	int32_t        src_rate;
	int32_t        pos;     /* current position in fixed-point (pos >> 16 = index) */
	int32_t        step;    /* fixed-point step per output sample */
	int32_t        lvol;    /* left  volume 0..127 */
	int32_t        rvol;    /* right volume 0..127 */
	int            active;
} channel_t;

static channel_t g_ch[NUM_CHANNELS];
static int       g_audio_fd = -1;
static int16_t   g_mix[TICK_SAMPLES * 2];  /* stereo interleaved */

/* ------------------------------------------------------------------ */

void
I_InitSound(boolean use_sfx_prefix)
{
	(void)use_sfx_prefix;

	g_audio_fd = audio_open("devfs:/audio");
	if (g_audio_fd < 0) {
		fprintf(stderr, "doom: I_InitSound: cannot open devfs:/audio\n");
		return;
	}
	audio_set_rate(g_audio_fd, MIX_RATE);
	memset(g_ch, 0, sizeof(g_ch));
}

void
I_ShutdownSound(void)
{
	if (g_audio_fd >= 0) {
		audio_close(g_audio_fd);
		g_audio_fd = -1;
	}
}

int
I_GetSfxLumpNum(sfxinfo_t *sfx)
{
	char name[16];
	snprintf(name, sizeof(name), "ds%s", sfx->name);
	return W_CheckNumForName(name);
}

int
I_StartSound(sfxinfo_t *sfx, int channel, int vol, int sep)
{
	if (g_audio_fd < 0)
		return -1;
	if (channel < 0 || channel >= NUM_CHANNELS)
		return -1;

	/* Cache the WAD lump data on first use via driver_data */
	if (sfx->driver_data == NULL) {
		int lump = I_GetSfxLumpNum(sfx);
		if (lump < 0)
			return -1;
		sfx->driver_data = W_CacheLumpNum(lump, PU_STATIC);
	}

	const uint8_t *raw = (const uint8_t *)sfx->driver_data;
	snd_hdr_t hdr;
	hdr.format = (uint16_t)(raw[0] | (raw[1] << 8));
	hdr.rate   = (uint16_t)(raw[2] | (raw[3] << 8));
	hdr.count  = (uint32_t)(raw[4] | (raw[5]<<8) | (raw[6]<<16) | (raw[7]<<24));

	if (hdr.format != 3 || hdr.count == 0)
		return -1;

	channel_t *ch = &g_ch[channel];
	ch->data     = raw + 8;
	ch->len      = (int32_t)hdr.count;
	ch->src_rate = hdr.rate ? (int32_t)hdr.rate : 11025;
	ch->pos      = 0;
	/* Fixed-point step: how many source samples advance per output sample */
	ch->step     = (int32_t)((int64_t)ch->src_rate * 65536 / MIX_RATE);

	/*
	 * sep=0 → full left, sep=128 → centre, sep=255 → full right.
	 * Scale vol (0-127) by the left/right fractions.
	 */
	ch->lvol  = (vol * (255 - sep)) >> 8;
	ch->rvol  = (vol * sep)         >> 8;
	ch->active = 1;
	return channel;
}

void
I_StopSound(int handle)
{
	if (handle >= 0 && handle < NUM_CHANNELS)
		g_ch[handle].active = 0;
}

boolean
I_SoundIsPlaying(int handle)
{
	if (handle < 0 || handle >= NUM_CHANNELS)
		return false;
	return g_ch[handle].active ? true : false;
}

void
I_UpdateSoundParams(int handle, int vol, int sep)
{
	if (handle < 0 || handle >= NUM_CHANNELS)
		return;
	g_ch[handle].lvol = (vol * (255 - sep)) >> 8;
	g_ch[handle].rvol = (vol * sep)         >> 8;
}

/*
 * I_UpdateSound — mix one game tick of audio and write to /dev/audio.
 * Called by doomgeneric once per 35 Hz tick.
 */
void
I_PrecacheSounds(sfxinfo_t *sounds, int num_sounds)
{
	(void)sounds;
	(void)num_sounds;
}

void
I_UpdateSound(void)
{
	if (g_audio_fd < 0)
		return;

	memset(g_mix, 0, sizeof(g_mix));

	for (int c = 0; c < NUM_CHANNELS; c++) {
		channel_t *ch = &g_ch[c];
		if (!ch->active)
			continue;

		for (int s = 0; s < TICK_SAMPLES; s++) {
			int32_t idx = ch->pos >> 16;
			if (idx >= ch->len) {
				ch->active = 0;
				break;
			}
			/* u8 → s16: (x - 128) * 256 gives [-32768, 32512] */
			int32_t samp = ((int32_t)ch->data[idx] - 128) * 256;

			int32_t l = (int32_t)g_mix[s * 2]     + ((samp * ch->lvol) >> 7);
			int32_t r = (int32_t)g_mix[s * 2 + 1] + ((samp * ch->rvol) >> 7);

			/* Clamp to int16_t */
			g_mix[s * 2]     = (int16_t)(l < -32768 ? -32768 : l > 32767 ? 32767 : l);
			g_mix[s * 2 + 1] = (int16_t)(r < -32768 ? -32768 : r > 32767 ? 32767 : r);

			ch->pos += ch->step;
		}
	}

	/* Mix OPL2 music into the same buffer before writing to the ring. */
	void I_MusicMix(int16_t *buf, int n);
	I_MusicMix(g_mix, TICK_SAMPLES);

	audio_write(g_audio_fd, g_mix, TICK_SAMPLES * 4);
}
