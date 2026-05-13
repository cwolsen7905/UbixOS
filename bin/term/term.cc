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

extern "C" {
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sys.h>
#include <sys/sched.h>
#include <sys/mpi.h>
#include <sys/kbd.h>
#include <views/display_proto.h>
}
#include <objgfx/objgfx.h>
#include <objgfx/ogFont.h>
#include <objgfx/ogPixelFmt.h>

#define TERM_W      640
#define TERM_H      400
#define TERM_BG     0x00101010u
#define TERM_FG_R   0xC0u
#define TERM_FG_G   0xC0u
#define TERM_FG_B   0xC0u
#define FONT_PATH   "sys:/lib/fonts/ROM8X8.DPF"

/* ------------------------------------------------------------------ */
/* Screen cell grid                                                     */
/* ------------------------------------------------------------------ */

static ogSurface  g_surf;
static ogBitFont  g_font;
static uint32_t   g_win_id;
static void      *g_shm     = NULL;
static char       g_mbox[]  = "term";
static char       g_views[] = "views";

static int  g_cols, g_rows;
static int  g_cur_col = 0, g_cur_row = 0;

/* Simple circular line buffer — each cell is one character */
#define MAX_ROWS 128
static char g_lines[MAX_ROWS][256];
static int  g_top = 0;   /* index of first visible row */

static void
term_redraw(void)
{
	g_surf.ogFillRect(0, 0, TERM_W - 1, TERM_H - 1, TERM_BG);

	int fw = (int)g_font.GetWidth();
	int fh = (int)g_font.GetHeight();

	g_font.SetFGColor(TERM_FG_R, TERM_FG_G, TERM_FG_B, 255);
	g_font.SetBGColor(
	    (TERM_BG >> 16) & 0xFF,
	    (TERM_BG >>  8) & 0xFF,
	     TERM_BG        & 0xFF,
	    255);

	for (int r = 0; r < g_rows; r++) {
		int line_idx = (g_top + r) % MAX_ROWS;
		int y = r * fh;
		g_font.PutString(g_surf, 0, y, g_lines[line_idx]);
		(void)fw;
	}

	/* cursor block */
	int cx = g_cur_col * (int)g_font.GetWidth();
	int cy = g_cur_row * (int)g_font.GetHeight();
	g_surf.ogFillRect(cx, cy, cx + (int)g_font.GetWidth() - 1,
	    cy + (int)g_font.GetHeight() - 1, 0x00C0C0C0u);
}

static void
term_scroll(void)
{
	g_top = (g_top + 1) % MAX_ROWS;
	memset(g_lines[(g_top + g_rows - 1) % MAX_ROWS], 0, g_cols + 1);
	g_cur_row = g_rows - 1;
}

static void
term_putchar(char c)
{
	if (c == '\n' || c == '\r') {
		g_cur_col = 0;
		if (g_cur_row < g_rows - 1)
			g_cur_row++;
		else
			term_scroll();
		return;
	}
	if (c == '\b') {
		if (g_cur_col > 0) {
			g_cur_col--;
			g_lines[(g_top + g_cur_row) % MAX_ROWS][g_cur_col] = ' ';
		}
		return;
	}

	int line_idx = (g_top + g_cur_row) % MAX_ROWS;
	if (g_cur_col < g_cols) {
		g_lines[line_idx][g_cur_col] = c;
		g_cur_col++;
	}
	if (g_cur_col >= g_cols) {
		g_cur_col = 0;
		if (g_cur_row < g_rows - 1)
			g_cur_row++;
		else
			term_scroll();
	}
}

static void
term_puts(const char *s)
{
	while (*s)
		term_putchar(*s++);
}

/* ------------------------------------------------------------------ */
/* Key → ASCII                                                          */
/* ------------------------------------------------------------------ */

static char
key_to_ascii(uint32_t kc, uint8_t pressed)
{
	if (!pressed)
		return 0;
	if (kc >= 0x20 && kc < 0x7F)
		return (char)kc;
	if (kc == '\n' || kc == '\r')
		return '\n';
	if (kc == '\b')
		return '\b';
	return 0;
}

/* ------------------------------------------------------------------ */
/* Simple line-input buffer                                             */
/* ------------------------------------------------------------------ */

#define LINEBUF 255
static char g_linebuf[LINEBUF + 1];
static int  g_linelen = 0;

static void
linebuf_flush(void)
{
	g_linebuf[g_linelen] = '\0';
	term_puts(g_linebuf);
	term_putchar('\n');
	/* TODO: pipe g_linebuf to shell subprocess */
	g_linelen = 0;
}

/* ------------------------------------------------------------------ */
/* MPI helpers                                                          */
/* ------------------------------------------------------------------ */

static void
send_flip(void)
{
	mpi_message_t msg;
	struct display_flip *fl = (struct display_flip *)msg.data;
	msg.header    = DISPLAY_FLIP;
	fl->window_id = g_win_id;
	fl->dirty_x   = 0;
	fl->dirty_y   = 0;
	fl->dirty_w   = 0;
	fl->dirty_h   = 0;
	mpi_postMessage(g_views, DISPLAY_FLIP, &msg);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int
main(int argc, char **argv)
{
	(void)argc; (void)argv;

	if (mpi_createMbox(g_mbox) != 0)
		return 1;

	/* Claim window */
	mpi_message_t msg;
	struct display_claim_req *creq = (struct display_claim_req *)msg.data;
	msg.header       = DISPLAY_CLAIM;
	creq->x          = 20;
	creq->y          = 20;
	creq->w          = TERM_W;
	creq->h          = TERM_H;
	creq->sender_pid = getpid();
	strncpy(creq->title, "Terminal", sizeof(creq->title) - 1);
	creq->title[sizeof(creq->title) - 1] = '\0';
	strncpy(creq->reply, g_mbox, sizeof(creq->reply) - 1);
	creq->reply[sizeof(creq->reply) - 1] = '\0';
	mpi_postMessage(g_views, DISPLAY_CLAIM, &msg);

	mpi_message_t reply;
	while (mpi_fetchMessage(g_mbox, &reply) != 0)
		sched_yield();

	if (reply.header != DISPLAY_ACK)
		return 1;

	struct display_ack *da = (struct display_ack *)reply.data;
	g_win_id = da->window_id;
	g_shm    = da->shm_base;

	if (!g_shm)
		return 1;

	/* Attach ogSurface to the shared window buffer */
	g_surf.ogAttach(g_shm, TERM_W, TERM_H, OG_PIXFMT_32BPP);

	/* Load bitmap font */
	if (!g_font.Load(FONT_PATH, 0)) {
		/* fallback: just clear to bg and show a solid block */
		g_surf.ogFillRect(0, 0, TERM_W - 1, TERM_H - 1, TERM_BG);
		send_flip();
		return 1;
	}

	g_cols = TERM_W / (int)g_font.GetWidth();
	g_rows = TERM_H / (int)g_font.GetHeight();
	if (g_rows > MAX_ROWS) g_rows = MAX_ROWS;

	memset(g_lines, 0, sizeof(g_lines));

	term_puts("UbixOS Terminal\n");
	term_puts("# ");

	term_redraw();
	send_flip();

	for (;;) {
		while (mpi_fetchMessage(g_mbox, &reply) == 0) {
			if (reply.header != DISPLAY_KEY)
				continue;

			struct display_key *dk = (struct display_key *)reply.data;
			char ch = key_to_ascii(dk->keycode, dk->pressed);
			if (ch == 0)
				continue;

			if (ch == '\n') {
				linebuf_flush();
				term_puts("# ");
			} else if (ch == '\b') {
				if (g_linelen > 0) {
					g_linelen--;
					term_putchar('\b');
				}
			} else if (g_linelen < LINEBUF) {
				g_linebuf[g_linelen++] = ch;
				term_putchar(ch);
			}

			term_redraw();
			send_flip();
		}

		sched_yield();
	}

	return 0;
}
