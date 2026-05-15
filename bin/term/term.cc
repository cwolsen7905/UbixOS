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

#include <algorithm>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <ubix/mailbox.hh>
#include <ubix/sched.hh>
#include <ubix/process.hh>
#include <views/display.hh>
#include <objgfx/objgfx.h>
#include <objgfx/ogFont.h>
#include <objgfx/ogPixelFmt.h>

#define TERM_W      640
#define TERM_H      400
#define TERM_BG     0x00101010u
#define TERM_FG_R   0xC0u
#define TERM_FG_G   0xC0u
#define TERM_FG_B   0xC0u
#define FONT_PATH   "sys:/var/fonts/ROM8X8.DPF"

/* ------------------------------------------------------------------ */
/* Screen cell grid                                                     */
/* ------------------------------------------------------------------ */

static ogSurface  g_surf;
static ogBitFont  g_font;
static uint32_t   g_win_id;
static void      *g_shm     = NULL;
static ubix::Mailbox g_mbox;
static const char    g_views[] = "views";

static int  g_cols, g_rows;
static int  g_act_w, g_act_h;
static int  g_cur_col = 0, g_cur_row = 0;

static std::vector<std::string> g_lines;

static void
term_redraw(void)
{
	int fw = (int)g_font.GetWidth();
	int fh = (int)g_font.GetHeight();

	g_surf.ogFillRect(0, 0, g_act_w - 1, g_act_h - 1, TERM_BG);

	g_font.SetFGColor(TERM_FG_R, TERM_FG_G, TERM_FG_B, 255);
	g_font.SetBGColor(
	    (TERM_BG >> 16) & 0xFF,
	    (TERM_BG >>  8) & 0xFF,
	     TERM_BG        & 0xFF,
	    255);

	int nrows = (int)g_lines.size();
	for (int r = 0; r < nrows; r++) {
		int y = r * fh;
		g_font.PutString(g_surf, 0, y, g_lines[r].c_str());
		(void)fw;
	}

	/* cursor block */
	int cx = g_cur_col * fw;
	int cy = g_cur_row * fh;
	g_surf.ogFillRect(cx, cy, cx + fw - 1, cy + fh - 1, 0x00C0C0C0u);

}

static void
term_scroll(void)
{
	g_lines.erase(g_lines.begin());
	g_lines.push_back(std::string(g_cols, ' '));
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
			g_lines[g_cur_row][g_cur_col] = ' ';
		}
		return;
	}

	if (g_cur_col < g_cols) {
		g_lines[g_cur_row][g_cur_col] = c;
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
/* Shell subprocess                                                     */
/* ------------------------------------------------------------------ */

static ubix::Shell g_shell;

/* ------------------------------------------------------------------ */
/* Simple line-input buffer                                             */
/* ------------------------------------------------------------------ */

static std::string g_linebuf;

static void
linebuf_flush(void)
{
	g_linebuf += '\n';
	g_shell.write(g_linebuf.c_str(), (int)g_linebuf.size());
	g_linebuf.clear();
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
	fl->dirty_w   = g_act_w;
	fl->dirty_h   = g_act_h;
	ubix::post_message(g_views, DISPLAY_FLIP, msg);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int
main(int argc, char **argv)
{
	(void)argc; (void)argv;

	g_mbox.assign("term." + std::to_string(ubix::pid()));
	if (!g_mbox.create())
		return 1;

	/* Fork the shell before claiming any shared memory so COW never
	 * marks shared pages read-only and breaks the mapping. */
	g_shell.spawn("sys:/bin/shell");

	/* Claim window */
	mpi_message_t msg = {};
	struct display_claim_req *creq = (struct display_claim_req *)msg.data;
	msg.header       = DISPLAY_CLAIM;
	creq->x          = 20;
	creq->y          = 20;
	creq->w          = TERM_W;
	creq->h          = TERM_H;
	creq->sender_pid = ubix::pid();
	std::strncpy(creq->title, "Terminal", sizeof(creq->title) - 1);
	creq->title[sizeof(creq->title) - 1] = '\0';
	std::strncpy(creq->reply, g_mbox.c_str(), sizeof(creq->reply) - 1);
	creq->reply[sizeof(creq->reply) - 1] = '\0';
	ubix::post_message(g_views, DISPLAY_CLAIM, msg);

	mpi_message_t reply;
	while (!g_mbox.try_fetch(reply))
		ubix::yield();

	if (reply.header != DISPLAY_ACK)
		return 1;

	struct display_ack *da = (struct display_ack *)reply.data;
	g_win_id = da->window_id;
	g_shm    = da->shm_base;
	g_act_w  = da->w;
	g_act_h  = da->h;

	if (!g_shm || g_act_w <= 0 || g_act_h <= 0)
		return 1;

	/* Attach ogSurface to the shared window buffer using actual dimensions */
	g_surf.ogAttach(g_shm, (uint32_t)g_act_w, (uint32_t)g_act_h, OG_PIXFMT_32BPP);

	/* Load bitmap font */
	if (!g_font.Load(FONT_PATH, 0)) {
		/* fallback: just clear to bg and show a solid block */
		g_surf.ogFillRect(0, 0, g_act_w - 1, g_act_h - 1, TERM_BG);
		send_flip();
		return 1;
	}

	g_cols = g_act_w / (int)g_font.GetWidth();
	g_rows = g_act_h / (int)g_font.GetHeight();
	g_lines.assign(g_rows, std::string(g_cols, ' '));

	/* Poll shell output without blocking the event loop */
	g_shell.set_nonblock();

	term_redraw();
	send_flip();

	int dirty = 0;
	for (;;) {
		/* Drain shell output */
		if (g_shell.valid()) {
			char outbuf[128];
			int n = g_shell.read(outbuf, sizeof(outbuf) - 1);
			if (n > 0) {
				outbuf[n] = '\0';
				term_puts(outbuf);
				dirty = 1;
			}
		}

		/* Process keyboard events */
		while (g_mbox.try_fetch(reply)) {
			if (reply.header == DISPLAY_CLOSE) {
				g_shell.kill();
				g_shell.close_fds();
				g_mbox.destroy();
				return 0;
			}
			if (reply.header != DISPLAY_KEY)
				continue;

			struct display_key *dk = (struct display_key *)reply.data;
			char ch = key_to_ascii(dk->keycode, dk->pressed);
			if (ch == 0)
				continue;

			if (ch == '\n') {
				term_putchar('\n');
				linebuf_flush();
			} else if (ch == '\b') {
				if (!g_linebuf.empty()) {
					g_linebuf.pop_back();
					term_putchar('\b');
				}
			} else {
				g_linebuf += ch;
				term_putchar(ch);
			}
			dirty = 1;
		}

		if (dirty) {
			term_redraw();
			send_flip();
			dirty = 0;
		}

		ubix::yield();
	}

	return 0;
}
