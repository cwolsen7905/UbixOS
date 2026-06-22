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
 * bin/hello — the canonical "Hello, views" reference app.
 *
 * Demonstrates the smallest complete views client:
 *   1. Create an MPI mailbox and CLAIM a window from the compositor.
 *   2. Attach an ogSurface to the granted shared buffer.
 *   3. Paint (gradient + centred text), then FLIP.
 *   4. Pump events: redraw on WINRESIZE, exit on CLOSE.
 *
 * Kept deliberately short and dependency-light so the docs/apps/ tutorial
 * can walk through it line-by-line.
 */

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mpi.h>
#include <sys/sched.h>
#include <views/display_proto.h>
}
#include <objgfx/objgfx.h>
#include <objgfx/ogScalableFont.h>

#define WIN_W 480
#define WIN_H 320
#define FONT_PATH "/var/fonts/DejaVuSans.ttf"
#define FONT_PX 28

#define RGB(r, g, b) ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))

/* Per-instance reply mailbox ("hello.<pid>"); a fixed name collides on the second
 * instance's mpi_createMbox.  Filled in by claim_window(). */
static char g_mbox[32] = "hello";
static const char g_views[] = "views";

static uint32_t win_id;
static int32_t win_w = WIN_W;
static int32_t win_h = WIN_H;
static ogSurface surf;
static ogScalableFont font;

/**
 * Claim a window from the compositor and attach our surface.
 *
 * @return 0 on success, -1 if the compositor denies the request.
 */
static int
claim_window(void)
{
	snprintf(g_mbox, sizeof(g_mbox), "hello.%d", (int)getpid());
	if (mpi_createMbox((char *)g_mbox) != 0) {
		printf("hello: mpi_createMbox failed\n");
		return -1;
	}

	mpi_message_t msg;
	memset(&msg, 0, sizeof(msg));
	msg.header = DISPLAY_CLAIM;

	struct display_claim_req *req = (struct display_claim_req *)msg.data;
	req->x = 0;
	req->y = 0;
	req->w = WIN_W;
	req->h = WIN_H;
	req->sender_pid = getpid();
	strncpy(req->title, "Hello, views", sizeof(req->title) - 1);
	strncpy(req->reply, g_mbox, sizeof(req->reply) - 1);
	/* Allow the user to resize from WIN_W x WIN_H up to 1280 x 720. */
	req->min_w = WIN_W;
	req->min_h = WIN_H;
	req->max_w = 1280;
	req->max_h = 720;

	while (mpi_postMessage((char *)g_views, DISPLAY_CLAIM, &msg) != 0)
		sched_yield();

	mpi_message_t reply;
	while (mpi_fetchMessage((char *)g_mbox, &reply) != 0)
		sched_yield();

	if (reply.header != DISPLAY_ACK) {
		printf("hello: compositor denied window\n");
		return -1;
	}

	struct display_ack *ack = (struct display_ack *)reply.data;
	win_id = ack->window_id;
	win_w = ack->w;
	win_h = ack->h;
	surf.ogAttach(ack->shm_base, (uint32_t)win_w, (uint32_t)win_h, OG_PIXFMT_32BPP);
	return 0;
}

/** Tell the compositor to recomposite the whole window. */
static void
flip(void)
{
	mpi_message_t msg;
	memset(&msg, 0, sizeof(msg));
	msg.header = DISPLAY_FLIP;

	struct display_flip *fl = (struct display_flip *)msg.data;
	fl->window_id = win_id;
	fl->dirty_x = 0;
	fl->dirty_y = 0;
	fl->dirty_w = win_w;
	fl->dirty_h = win_h;
	mpi_postMessage((char *)g_views, DISPLAY_FLIP, &msg);
}

/** Paint one frame: a top-to-bottom blue gradient plus centred text. */
static void
render(void)
{
	for (int32_t y = 0; y < win_h; y++) {
		uint8_t b = (uint8_t)(40 + (180 * y) / (win_h ? win_h : 1));
		surf.ogHLine(0, win_w - 1, y, RGB(20, 40, b));
	}

	if (font.IsValid()) {
		font.SetFGColor(255, 255, 255);
		const char *msg = "Hello, views";
		uint32_t tw = font.TextWidth(msg);
		int32_t tx = (win_w - (int32_t)tw) / 2;
		int32_t ty = (win_h - (int32_t)font.GetHeight()) / 2;
		font.PutString(surf, tx, ty, msg);
	}

	flip();
}

/** Release the window and tear down our mailbox before exiting. */
static void
release_window(void)
{
	mpi_message_t msg;
	memset(&msg, 0, sizeof(msg));
	msg.header = DISPLAY_RELEASE;

	struct display_release *rel = (struct display_release *)msg.data;
	rel->window_id = win_id;
	mpi_postMessage((char *)g_views, DISPLAY_RELEASE, &msg);
	mpi_destroyMbox((char *)g_mbox);
}

int
main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	if (claim_window() != 0)
		return 1;

	if (!font.Load(FONT_PATH, FONT_PX))
		printf("hello: %s not found; running without text\n", FONT_PATH);

	render();

	for (;;) {
		mpi_message_t ev;
		if (mpi_fetchMessage((char *)g_mbox, &ev) != 0) {
			sched_yield();
			continue;
		}

		switch (ev.header) {
		case DISPLAY_CLOSE:
			release_window();
			return 0;

		case DISPLAY_WINRESIZE: {
			struct display_winresize *wr = (struct display_winresize *)ev.data;
			win_w = wr->w;
			win_h = wr->h;
			surf.ogAttach(wr->shm_base, (uint32_t)wr->w, (uint32_t)wr->h, OG_PIXFMT_32BPP);
			render();
			break;
		}

		case DISPLAY_KEY: {
			struct display_key *k = (struct display_key *)ev.data;
			/* ESC quits, as a convenience. */
			if (k->pressed && k->keycode == 27) {
				release_window();
				return 0;
			}
			break;
		}

		default:
			/* DISPLAY_MOUSE and friends: ignored by this minimal example. */
			break;
		}
	}
}
