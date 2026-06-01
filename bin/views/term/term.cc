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
 * term — graphical terminal.  Runs the user's shell on a kernel pseudo-terminal
 * (ubix::Pty) so interactive shells such as tcsh behave exactly as on the text
 * console: line editing, history, job control, Ctrl-C, and color.  The kernel's
 * VT100 engine renders the shell's output into an 80x25 char+attribute cell
 * grid; term snapshots that grid each frame and draws it with objGFX, and feeds
 * keystrokes back in (encoded to VT100 byte sequences) via the pty.
 */

#include <string>
#include <cstring>
#include <stdlib.h>
#include <ubix/mailbox.hh>
#include <ubix/sched.hh>
#include <ubix/process.hh>
#include <views/display.hh>
#include <objgfx/objgfx.h>
#include <objgfx/ogScalableFont.h>
#include <objgfx/ogPixelFmt.h>
#include <sys/kbd.h>

#define DEF_COLS 80
#define DEF_ROWS 25
#define TERM_FW 8 /* ROM8X14 cell size */
#define TERM_FH 14
#define TERM_MAX_COLS 200 /* matches kernel TTY_MAX_COLS */
#define TERM_MAX_ROWS 64  /* matches kernel TTY_MAX_ROWS */
#define MAX_CELL_BYTES (TERM_MAX_COLS * TERM_MAX_ROWS * 2)
#define TERM_W (DEF_COLS * TERM_FW) /* 640 */
#define TERM_H (DEF_ROWS * TERM_FH) /* 350 */
#define TERM_BG 0x00000000u
#define FONT_PATH "/var/fonts/DejaVuSansMono.ttf"

/* Current grid dimensions (cells); change on a live window resize. */
static int g_cols = DEF_COLS;
static int g_rows = DEF_ROWS;

/* CGA/VGA 16-colour text palette → 0x00RRGGBB. */
static const uint32_t g_vga_palette[16] = {0x00000000,
                                           0x000000AA,
                                           0x0000AA00,
                                           0x0000AAAA,
                                           0x00AA0000,
                                           0x00AA00AA,
                                           0x00AA5500,
                                           0x00AAAAAA,
                                           0x00555555,
                                           0x005555FF,
                                           0x0055FF55,
                                           0x0055FFFF,
                                           0x00FF5555,
                                           0x00FF55FF,
                                           0x00FFFF55,
                                           0x00FFFFFF};

/* ------------------------------------------------------------------ */
/* TerminalView — draws a snapshotted 80x25 char+attribute cell grid    */
/* ------------------------------------------------------------------ */

/* Damaged region of the surface, in inclusive pixel bounds. */
struct DirtyRect
{
	int x0, y0, x1, y1;
	bool any;
};

class TerminalView
{
	ogSurface surf_;
	ogScalableFont font_;
	int fw_ = TERM_FW;
	int fh_ = TERM_FH;
	int sw_ = 0; /* attached surface dimensions */
	int sh_ = 0;
	uint32_t cur_bg_ = 0; /* current cell background (filled per cell) */

	unsigned char prev_[MAX_CELL_BYTES]; /* last rendered grid, for cell diffing */
	int prev_cx_ = -1, prev_cy_ = -1;    /* last cursor cell */
	bool force_full_ = true;             /* clear + redraw everything next render */
	bool prev_exited_ = false;           /* exit banner already drawn */
	DirtyRect dirty_ = {0, 0, 0, 0, false};

	/* Grow the accumulated damage rect to include a cell-sized box. */
	void mark(int px, int py, int pw, int ph)
	{
		int x1 = px + pw - 1, y1 = py + ph - 1;
		if (!dirty_.any)
		{
			dirty_ = {px, py, x1, y1, true};
			return;
		}
		if (px < dirty_.x0)
			dirty_.x0 = px;
		if (py < dirty_.y0)
			dirty_.y0 = py;
		if (x1 > dirty_.x1)
			dirty_.x1 = x1;
		if (y1 > dirty_.y1)
			dirty_.y1 = y1;
	}

	/* Select the VT100 attribute: the glyph foreground is set on the font; the
	 * cell background is remembered so the cell can be filled.  The font
	 * background is kept transparent so the antialiased glyph blends over it. */
	void set_attr(unsigned char attr)
	{
		uint32_t fg = g_vga_palette[attr & 0x0F];
		cur_bg_ = g_vga_palette[(attr >> 4) & 0x07];
		font_.SetFGColor((fg >> 16) & 0xFF, (fg >> 8) & 0xFF, fg & 0xFF, 255);
		font_.SetBGColor(0, 0, 0, 0);
	}

	/* Draw one cell with its normal VT100 colours (no cursor) and damage it. */
	void draw_cell(const unsigned char *cells, int row, int col)
	{
		int idx = (row * g_cols + col) * 2;
		unsigned char ch = cells[idx];
		unsigned char attr = cells[idx + 1];
		if (ch == 0)
			ch = ' ';
		set_attr(attr);
		int px = col * fw_, py = row * fh_;
		surf_.ogFillRect(px, py, px + fw_ - 1, py + fh_ - 1, cur_bg_);
		if (ch != ' ')
			font_.PutChar(surf_, px, py, (char)ch);
		mark(px, py, fw_, fh_);
	}

	/* Draw the block cursor (inverted glyph on a light cell) and damage it. */
	void draw_cursor(const unsigned char *cells, int row, int col)
	{
		int px = col * fw_, py = row * fh_;
		surf_.ogFillRect(px, py, px + fw_ - 1, py + fh_ - 1, 0x00AAAAAAu);
		unsigned char ch = cells[(row * g_cols + col) * 2];
		if (ch != 0 && ch != ' ')
		{
			font_.SetFGColor(0, 0, 0, 255);
			font_.SetBGColor(0, 0, 0, 0);
			font_.PutChar(surf_, px, py, (char)ch);
		}
		mark(px, py, fw_, fh_);
	}

      public:
	bool attach(void *shm, int w, int h)
	{
		sw_ = w;
		sh_ = h;
		std::memset(prev_, 0xFF, sizeof(prev_)); /* force a full redraw */
		prev_cx_ = prev_cy_ = -1;
		force_full_ = true;
		return surf_.ogAttach(shm, (uint32_t)w, (uint32_t)h, OG_PIXFMT_32BPP);
	}

	bool load_font(const char *path)
	{
		/* The cell grid is pinned to a fixed TERM_FW x TERM_FH so the resize
		 * math and the kernel pty geometry stay consistent; the monospace glyph
		 * is rendered within that cell (its natural advance is <= TERM_FW). */
		return font_.Load(path, TERM_FH);
	}

	int fw() const
	{
		return fw_;
	}
	int fh() const
	{
		return fh_;
	}

	/*
	 * Render the cell grid with a block cursor at (cx,cy), redrawing only the
	 * cells that changed since the previous frame.  Returns the damaged pixel
	 * rect (any == false when nothing changed, so the caller can skip the flip).
	 * When exited is set the cursor is hidden and a banner overlays the bottom
	 * row; the final screen is drawn once and then stays quiet.
	 */
	DirtyRect render(const unsigned char *cells, int cx, int cy, bool exited)
	{
		dirty_ = {0, 0, 0, 0, false};
		int cell_bytes = g_cols * g_rows * 2;

		if (exited)
		{
			if (prev_exited_ && !force_full_)
				return dirty_; /* banner already up, nothing to do */
			for (int row = 0; row < g_rows; row++)
				for (int col = 0; col < g_cols; col++)
					draw_cell(cells, row, col);
			static const char msg[] = " [process exited - press any key to close]";
			int by = (g_rows - 1) * fh_;
			surf_.ogFillRect(0, by, g_cols * fw_ - 1, by + fh_ - 1, 0x00701010u);
			font_.SetFGColor(0xFF, 0xFF, 0xFF, 255);
			font_.SetBGColor(0, 0, 0, 0);
			font_.PutString(surf_, 0, by, msg);
			mark(0, 0, sw_, sh_);
			prev_exited_ = true;
			force_full_ = false;
			std::memcpy(prev_, cells, (size_t)cell_bytes);
			return dirty_;
		}

		if (force_full_)
		{
			/* Clear the whole surface (covers the margin when the window size is
			 * not an exact multiple of the cell size) and redraw every cell. */
			surf_.ogFillRect(0, 0, sw_ - 1, sh_ - 1, TERM_BG);
			for (int row = 0; row < g_rows; row++)
				for (int col = 0; col < g_cols; col++)
					draw_cell(cells, row, col);
			if (cx >= 0 && cx < g_cols && cy >= 0 && cy < g_rows)
				draw_cursor(cells, cy, cx);
			mark(0, 0, sw_, sh_);
			force_full_ = false;
			std::memcpy(prev_, cells, (size_t)cell_bytes);
			prev_cx_ = cx;
			prev_cy_ = cy;
			return dirty_;
		}

		/* Incremental: redraw only cells whose char or attribute changed. */
		bool cursor_cell_redrawn = false;
		for (int row = 0; row < g_rows; row++)
		{
			for (int col = 0; col < g_cols; col++)
			{
				int idx = (row * g_cols + col) * 2;
				if (cells[idx] == prev_[idx] && cells[idx + 1] == prev_[idx + 1])
					continue;
				draw_cell(cells, row, col);
				if (row == cy && col == cx)
					cursor_cell_redrawn = true;
			}
		}

		/* Cursor: restore the vacated cell, then (re)draw the block where the
		 * cursor now is — but only when it moved or its cell was repainted, so an
		 * idle terminal produces no damage and no flip. */
		bool cursor_moved = (cx != prev_cx_ || cy != prev_cy_);
		if (cursor_moved && prev_cx_ >= 0 && prev_cx_ < g_cols && prev_cy_ >= 0 && prev_cy_ < g_rows)
			draw_cell(cells, prev_cy_, prev_cx_);
		if (cx >= 0 && cx < g_cols && cy >= 0 && cy < g_rows && (cursor_moved || cursor_cell_redrawn))
			draw_cursor(cells, cy, cx);

		std::memcpy(prev_, cells, (size_t)cell_bytes);
		prev_cx_ = cx;
		prev_cy_ = cy;
		return dirty_;
	}
};

/* ------------------------------------------------------------------ */
/* Key → VT100 byte sequence                                            */
/* ------------------------------------------------------------------ */

/*
 * Encode a compositor key event into the byte(s) a terminal program expects.
 * Printable and control characters (Ctrl-C = 0x03 etc.) arrive pre-folded as
 * keycodes < 0x100 and pass straight through; KEY_* specials map to VT100
 * escape sequences.
 *
 * @param out  buffer of at least 8 bytes.
 * @return number of bytes written (0 to ignore the event).
 */
static int key_to_seq(uint32_t kc, uint8_t pressed, char *out)
{
	if (!pressed)
		return 0;
	if (kc >= KEY_LSHIFT && kc <= KEY_LALT)
		return 0; /* modifier press/release — not a character */

	if (kc < 0x100)
	{
		out[0] = (char)kc;
		return 1;
	}

	const char *seq = nullptr;
	switch (kc)
	{
		case KEY_UP:
			seq = "\x1b[A";
			break;
		case KEY_DOWN:
			seq = "\x1b[B";
			break;
		case KEY_RIGHT:
			seq = "\x1b[C";
			break;
		case KEY_LEFT:
			seq = "\x1b[D";
			break;
		case KEY_HOME:
			seq = "\x1b[H";
			break;
		case KEY_END:
			seq = "\x1b[F";
			break;
		case KEY_PGUP:
			seq = "\x1b[5~";
			break;
		case KEY_PGDN:
			seq = "\x1b[6~";
			break;
		case KEY_INS:
			seq = "\x1b[2~";
			break;
		case KEY_DEL:
			seq = "\x1b[3~";
			break;
		case KEY_F1:
			seq = "\x1bOP";
			break;
		case KEY_F2:
			seq = "\x1bOQ";
			break;
		case KEY_F3:
			seq = "\x1bOR";
			break;
		case KEY_F4:
			seq = "\x1bOS";
			break;
		case KEY_F5:
			seq = "\x1b[15~";
			break;
		case KEY_F6:
			seq = "\x1b[17~";
			break;
		case KEY_F7:
			seq = "\x1b[18~";
			break;
		case KEY_F8:
			seq = "\x1b[19~";
			break;
		case KEY_F9:
			seq = "\x1b[20~";
			break;
		case KEY_F10:
			seq = "\x1b[21~";
			break;
		case KEY_F11:
			seq = "\x1b[23~";
			break;
		case KEY_F12:
			seq = "\x1b[24~";
			break;
		default:
			return 0;
	}

	int n = 0;
	while (seq[n] != '\0')
	{
		out[n] = seq[n];
		n++;
	}
	return n;
}

/* ------------------------------------------------------------------ */
/* main — thin event loop wiring the pty to the display                 */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	if (!ubix::views_running())
	{
		printf("term: views compositor is not running\n");
		return 1;
	}

	ubix::Mailbox mbox;
	mbox.assign("term." + std::to_string(ubix::pid()));
	if (!mbox.create())
		return 1;

	/* Resolve the user's shell from the session env (set by vlogin from
	 * /etc/userdb); fall back to the native shell.  TERM=vt100 so tcsh and
	 * curses programs emit sequences the kernel VT100 engine understands. */
	const char *shell_path = getenv("SHELL");
	if (shell_path == nullptr || shell_path[0] == '\0')
		shell_path = "/bin/shell";

	const char *user = getenv("USER") ? getenv("USER") : "root";
	char shell_env[160];
	char home_env[160];
	char user_env[64];
	char logname_env[64];
	snprintf(shell_env, sizeof(shell_env), "SHELL=%s", shell_path);
	snprintf(home_env, sizeof(home_env), "HOME=%s", getenv("HOME") ? getenv("HOME") : "/");
	snprintf(user_env, sizeof(user_env), "USER=%s", user);
	snprintf(logname_env, sizeof(logname_env), "LOGNAME=%s", user);

	char *shell_argv[] = {(char *)shell_path, nullptr};
	char *shell_envp[] = {home_env,
	                      shell_env,
	                      user_env,
	                      logname_env,
	                      (char *)"PATH=/bin:/sbin:/usr/bin:/usr/sbin",
	                      (char *)"TERM=vt100",
	                      nullptr};

	ubix::Pty pty;
	if (!pty.spawn(shell_path, shell_argv, shell_envp))
	{
		printf("term: failed to start shell %s\n", shell_path);
		return 1;
	}

	mpi_message_t msg = {};
	struct display_claim_req *creq = (struct display_claim_req *)msg.data;
	msg.header = DISPLAY_CLAIM;
	creq->x = 20;
	creq->y = 20;
	creq->w = TERM_W;
	creq->h = TERM_H;
	/* Resizable: the grid reflows to whatever cells fit the window. */
	creq->min_w = 40 * TERM_FW;
	creq->min_h = 10 * TERM_FH;
	creq->max_w = 120 * TERM_FW; /* 960 — fits a 1024-wide screen */
	creq->max_h = 50 * TERM_FH;  /* 700 — fits a 768-tall screen  */
	creq->sender_pid = ubix::pid();
	std::strncpy(creq->title, "Terminal", sizeof(creq->title) - 1);
	std::strncpy(creq->reply, mbox.c_str(), sizeof(creq->reply) - 1);
	ubix::post_message("views", DISPLAY_CLAIM, msg);

	mpi_message_t reply;
	while (!mbox.try_fetch(reply))
		ubix::yield();

	if (reply.header != DISPLAY_ACK)
	{
		pty.kill();
		pty.release();
		return 1;
	}

	struct display_ack *da = (struct display_ack *)reply.data;
	uint32_t win_id = da->window_id;
	void *shm = da->shm_base;
	int act_w = da->w;
	int act_h = da->h;

	if (!shm || act_w <= 0 || act_h <= 0)
	{
		pty.kill();
		pty.release();
		return 1;
	}

	TerminalView tv;
	if (!tv.attach(shm, act_w, act_h) || !tv.load_font(FONT_PATH))
	{
		pty.kill();
		pty.release();
		return 1;
	}

	auto send_flip = [&](const DirtyRect &d)
	{
		/* Flip only the damaged rectangle, clamped to the surface. */
		int x0 = d.x0 < 0 ? 0 : d.x0;
		int y0 = d.y0 < 0 ? 0 : d.y0;
		int x1 = d.x1 >= act_w ? act_w - 1 : d.x1;
		int y1 = d.y1 >= act_h ? act_h - 1 : d.y1;
		if (x1 < x0 || y1 < y0)
			return;
		mpi_message_t m = {};
		struct display_flip *fl = (struct display_flip *)m.data;
		m.header = DISPLAY_FLIP;
		fl->window_id = win_id;
		fl->dirty_x = x0;
		fl->dirty_y = y0;
		fl->dirty_w = x1 - x0 + 1;
		fl->dirty_h = y1 - y0 + 1;
		ubix::post_message("views", DISPLAY_FLIP, m);
	};

	auto close_term = [&]()
	{
		mpi_message_t rel = {};
		struct display_release *dr = (struct display_release *)rel.data;
		rel.header = DISPLAY_RELEASE;
		dr->window_id = win_id;
		ubix::post_message("views", DISPLAY_RELEASE, rel);
		pty.kill();
		pty.release();
		mbox.destroy();
	};

	auto set_title = [&](const char *t)
	{
		mpi_message_t m = {};
		struct display_settitle *st = (struct display_settitle *)m.data;
		m.header = DISPLAY_SETTITLE;
		st->window_id = win_id;
		std::strncpy(st->title, t, sizeof(st->title) - 1);
		ubix::post_message("views", DISPLAY_SETTITLE, m);
	};

	unsigned char grid[MAX_CELL_BYTES];
	unsigned short cx16 = 0, cy16 = 0;
	bool shell_exited = false;

	for (;;)
	{
		/* Pull the latest rendered screen.  render() diffs it internally and
		 * redraws only the cells that changed. */
		bool got = (pty.snapshot(grid, &cx16, &cy16) == 0);

		/* Detect the shell exiting: mark the window and draw the banner once. */
		if (!shell_exited && pty.exited())
		{
			shell_exited = true;
			set_title("Terminal (exited)");
		}

		while (mbox.try_fetch(reply))
		{
			if (reply.header == DISPLAY_CLOSE)
			{
				close_term();
				return 0;
			}
			if (reply.header == DISPLAY_WINRESIZE)
			{
				struct display_winresize *wr = (struct display_winresize *)reply.data;
				int nc = wr->w / tv.fw();
				int nr = wr->h / tv.fh();
				if (nc < 40)
					nc = 40;
				if (nc > 120)
					nc = 120;
				if (nr < 10)
					nr = 10;
				if (nr > 50)
					nr = 50;
				pty.resize(nc, nr); /* resizes the grid + SIGWINCHes the shell */
				g_cols = nc;
				g_rows = nr;
				act_w = wr->w;
				act_h = wr->h;
				tv.attach(wr->shm_base, wr->w, wr->h); /* resets diff state, forces full redraw */
				continue;
			}
			if (reply.header != DISPLAY_KEY)
				continue;

			struct display_key *dk = (struct display_key *)reply.data;

			/* After the shell exits, any keypress closes the window. */
			if (shell_exited)
			{
				if (dk->pressed)
				{
					close_term();
					return 0;
				}
				continue;
			}

			char seq[8];
			int n = key_to_seq(dk->keycode, dk->pressed, seq);
			if (n > 0)
				pty.inject(seq, n);
		}

		/* The cursor is stored as a split linear offset (see tty_print). */
		int linear = (cy16 << 8) | cx16;
		int cx = linear % g_cols;
		int cy = linear / g_cols;

		if (got || shell_exited)
		{
			DirtyRect d = tv.render(grid, cx, cy, shell_exited);
			if (d.any)
				send_flip(d);
		}

		ubix::yield();
	}

	return 0;
}
