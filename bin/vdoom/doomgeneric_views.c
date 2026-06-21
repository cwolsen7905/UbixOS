/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors
 *    in the documentation and/or other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * doomgeneric_views.c — views compositor backend for DOOM (vdoom).
 *
 * Implements the 6-function DG_ API targeting the UbixOS views window
 * compositor rather than the raw VESA framebuffer:
 *
 *   DG_Init          — claim a 320×200 window via DISPLAY_CLAIM
 *   DG_DrawFrame     — blit DG_ScreenBuffer 1:1 to shm_base, then DISPLAY_FLIP
 *   DG_SleepMs       — usleep
 *   DG_GetTicksMs    — gettimeofday
 *   DG_GetKey        — drain MPI mailbox for DISPLAY_KEY events
 *   DG_SetWindowTitle — no-op (title set at DISPLAY_CLAIM time)
 *
 * Keyboard events arrive as DISPLAY_KEY MPI messages from the compositor.
 * The keycode field carries the same values as the raw kbd_ring (ASCII for
 * printable keys, KEY_* constants for specials), so to_doom_key() is
 * identical to the one in doomgeneric_ubix.c.
 *
 * The shared buffer (shm_base) is 32bpp 0x00RRGGBB — same format as
 * DG_ScreenBuffer — so each frame is a single memcpy per row.
 */

#include "doomgeneric.h"
#include "doomkeys.h"

#include <api/ubix.h>
#include <views/display_proto.h>
#include <sys/kbd.h>
#include <sys/mpi.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t g_win_id;
static uint32_t *g_shm;      /* shared pixel buffer, 32bpp 0x00RRGGBB */
static uint32_t g_shm_pitch; /* bytes per row in the shared buffer */
static uint32_t g_win_w;
static uint32_t g_win_h;
static uint32_t g_start_ms;

/* Per-instance reply mailbox name.  MUST be unique per process: a fixed "vdoom"
 * name makes mpi_createMbox() fail for a second instance (name already taken), so
 * only one DOOM could ever run.  DG_Init() fills in "vdoom.<pid>". */
static char g_mbox[32] = "vdoom";

/* ------------------------------------------------------------------ */

static uint32_t now_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint32_t)tv.tv_sec * 1000u + (uint32_t)tv.tv_usec / 1000u;
}

static void do_flip(void)
{
	mpi_message_t msg;
	struct display_flip *fl;

	memset(&msg, 0, sizeof(msg));
	msg.header = DISPLAY_FLIP;
	fl = (struct display_flip *)msg.data;
	fl->window_id = g_win_id;
	fl->dirty_x = 0;
	fl->dirty_y = 0;
	fl->dirty_w = (int32_t)g_win_w;
	fl->dirty_h = (int32_t)g_win_h;
	mpi_postMessage("views", DISPLAY_FLIP, &msg);
}

/* ------------------------------------------------------------------ */
/* DG_ platform functions                                               */
/* ------------------------------------------------------------------ */

void DG_Init(void)
{
	if (!views_running())
	{
		fprintf(stderr, "vdoom: views compositor is not running\n");
		printf("vdoom: views compositor is not running\n");
		exit(1);
	}

	mpi_message_t msg, reply;
	struct display_claim_req *req;
	struct display_ack *ack;

	/* Unique reply mailbox per process so multiple DOOM instances can each claim
	 * their own window (a fixed name collides on the second mpi_createMbox). */
	snprintf(g_mbox, sizeof(g_mbox), "vdoom.%d", (int)getpid());

	if (mpi_createMbox(g_mbox) != 0)
	{
		fprintf(stderr, "vdoom: mpi_createMbox failed\n");
		return;
	}

	memset(&msg, 0, sizeof(msg));
	msg.header = DISPLAY_CLAIM;
	req = (struct display_claim_req *)msg.data;
	req->x = 50;
	req->y = 50;
	req->w = DOOMGENERIC_RESX;
	req->h = DOOMGENERIC_RESY;
	req->sender_pid = getpid();
	strncpy(req->title, "DOOM", sizeof(req->title) - 1);
	strncpy(req->reply, g_mbox, sizeof(req->reply) - 1);

	while (mpi_postMessage("views", DISPLAY_CLAIM, &msg) != 0)
		usleep(5000);

	while (mpi_fetchMessage(g_mbox, &reply) != 0)
		usleep(1000);

	if (reply.header != DISPLAY_ACK)
	{
		fprintf(stderr, "vdoom: DISPLAY_DENIED\n");
		return;
	}

	ack = (struct display_ack *)reply.data;
	g_win_id = ack->window_id;
	g_shm = (uint32_t *)ack->shm_base;
	g_shm_pitch = ack->pitch;
	g_win_w = (uint32_t)ack->w;
	g_win_h = (uint32_t)ack->h;
	g_start_ms = now_ms();

	fprintf(
	    stderr, "vdoom: window %u %ux%u shm=%p pitch=%u\n", g_win_id, g_win_w, g_win_h, (void *)g_shm, g_shm_pitch);
}

/*
 * DG_DrawFrame — copy DOOM's 320×200 buffer 1:1 into the window.
 * No scaling: the window was claimed at exactly DOOMGENERIC_RESX×RESY.
 * The shared buffer may be wider than 320×4 bytes (pitch >= 1280), so
 * copy row by row.
 */
void DG_DrawFrame(void)
{
	if (!g_shm)
		return;

	const uint32_t *src = DG_ScreenBuffer;
	uint8_t *dst = (uint8_t *)g_shm;
	uint32_t rows = DOOMGENERIC_RESY;
	uint32_t row_bytes = DOOMGENERIC_RESX * sizeof(uint32_t);

	if (g_win_h < rows)
		rows = g_win_h;

	for (uint32_t y = 0; y < rows; y++)
	{
		memcpy(dst, src, row_bytes);
		src += DOOMGENERIC_RESX;
		dst += g_shm_pitch;
	}

	do_flip();
}

void DG_SleepMs(uint32_t ms)
{
	usleep(ms * 1000u);
}

uint32_t DG_GetTicksMs(void)
{
	return now_ms() - g_start_ms;
}

/*
 * Translate UbixOS keycode (from DISPLAY_KEY) to a doomgeneric key code.
 * Identical to to_doom_key() in doomgeneric_ubix.c — the compositor
 * forwards the same keycode values the raw kbd_ring produces.
 */
static unsigned char to_doom_key(uint32_t kc)
{
	switch (kc)
	{
		case KEY_UP:
			return KEY_UPARROW;
		case KEY_DOWN:
			return KEY_DOWNARROW;
		case KEY_LEFT:
			return KEY_LEFTARROW;
		case KEY_RIGHT:
			return KEY_RIGHTARROW;

		case KEY_LCTRL:
			return KEY_FIRE;
		case KEY_LALT:
			return KEY_RALT;
		case KEY_LSHIFT:
			return KEY_RSHIFT;

		case ' ':
			return KEY_USE;
		case KEY_ESC:
			return KEY_ESCAPE;
		case '\r':
		case '\n':
			return KEY_ENTER;
		case '\t':
			return KEY_TAB;
		case 0x7f:
		case '\b':
			return KEY_BACKSPACE;

		case KEY_F1:
			return (unsigned char)(0x80 + 0x3b);
		case KEY_F2:
			return (unsigned char)(0x80 + 0x3c);
		case KEY_F3:
			return (unsigned char)(0x80 + 0x3d);
		case KEY_F4:
			return (unsigned char)(0x80 + 0x3e);
		case KEY_F5:
			return (unsigned char)(0x80 + 0x3f);
		case KEY_F6:
			return (unsigned char)(0x80 + 0x40);
		case KEY_F7:
			return (unsigned char)(0x80 + 0x41);
		case KEY_F8:
			return (unsigned char)(0x80 + 0x42);
		case KEY_F9:
			return (unsigned char)(0x80 + 0x43);
		case KEY_F10:
			return (unsigned char)(0x80 + 0x44);
		case KEY_F11:
			return (unsigned char)(0x80 + 0x57);
		case KEY_F12:
			return (unsigned char)(0x80 + 0x58);

		default:
			if (kc >= 0x20 && kc < 0x80)
				return (unsigned char)kc;
			return 0;
	}
}

/*
 * DG_GetKey — drain one MPI message from our mailbox.
 *
 * DISPLAY_KEY  → convert keycode and return 1.
 * DISPLAY_CLOSE → the user closed the window; exit cleanly.
 * Anything else is discarded; returns 0 so the caller polls again.
 */
int DG_GetKey(int *pressed, unsigned char *doomKey)
{
	mpi_message_t msg;

	if (mpi_fetchMessage(g_mbox, &msg) != 0)
		return 0;

	if (msg.header == DISPLAY_CLOSE)
	{
		mpi_destroyMbox(g_mbox);
		exit(0);
	}

	if (msg.header == DISPLAY_KEY)
	{
		struct display_key *dk = (struct display_key *)msg.data;
		unsigned char k = to_doom_key(dk->keycode);
		if (k == 0)
			return 0;
		*pressed = dk->pressed;
		*doomKey = k;
		return 1;
	}

	return 0;
}

void DG_SetWindowTitle(const char *title)
{
	(void)title;
}

/* ------------------------------------------------------------------ */
/* Entry point                                                          */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
	freopen("/vdoom.log", "w", stderr);
	fprintf(stderr, "vdoom: starting\n");
	fflush(stderr);

	int has_iwad = 0;
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-iwad") == 0)
		{
			has_iwad = 1;
			break;
		}
	}
	static char default_iwad_path[] = "/bin/doom1.wad";
	static char *new_argv[64];
	int new_argc = argc;
	if (!has_iwad && argc < 62)
	{
		int i;
		for (i = 0; i < argc; i++)
			new_argv[i] = argv[i];
		new_argv[i++] = "-iwad";
		new_argv[i++] = default_iwad_path;
		new_argv[i] = NULL;
		new_argc = i;
		argv = new_argv;
	}

	doomgeneric_Create(new_argc, argv);

	for (;;)
		doomgeneric_Tick();

	return 0;
}
