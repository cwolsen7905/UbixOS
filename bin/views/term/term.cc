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
#include <objgfx/ogFont.h>
#include <objgfx/ogPixelFmt.h>
#include <sys/kbd.h>

#define GRID_COLS 80
#define GRID_ROWS 25
#define CELL_BYTES (GRID_COLS * GRID_ROWS * 2)
#define TERM_W (GRID_COLS * 8)  /* 640 with the 8x14 ROM font */
#define TERM_H (GRID_ROWS * 14) /* 350 */
#define TERM_BG 0x00000000u
#define FONT_PATH "/var/fonts/ROM8X14.DPF"

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

class TerminalView
{
	ogSurface surf_;
	ogBitFont font_;
	int fw_ = 8;
	int fh_ = 14;

	void set_attr(unsigned char attr)
	{
		uint32_t fg = g_vga_palette[attr & 0x0F];
		uint32_t bg = g_vga_palette[(attr >> 4) & 0x07];
		font_.SetFGColor((fg >> 16) & 0xFF, (fg >> 8) & 0xFF, fg & 0xFF, 255);
		font_.SetBGColor((bg >> 16) & 0xFF, (bg >> 8) & 0xFF, bg & 0xFF, 255);
	}

      public:
	bool attach(void *shm, int w, int h)
	{
		return surf_.ogAttach(shm, (uint32_t)w, (uint32_t)h, OG_PIXFMT_32BPP);
	}

	bool load_font(const char *path)
	{
		if (!font_.Load(path, 0))
			return false;
		fw_ = (int)font_.GetWidth();
		fh_ = (int)font_.GetHeight();
		return true;
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
	 * Render the cell grid with a block cursor at (cx,cy).  When exited is
	 * set, the cursor is hidden and a status banner overlays the bottom row.
	 */
	void render(const unsigned char *cells, int cx, int cy, bool exited)
	{
		surf_.ogFillRect(0, 0, GRID_COLS * fw_ - 1, GRID_ROWS * fh_ - 1, TERM_BG);

		int last_attr = -1;
		for (int row = 0; row < GRID_ROWS; row++)
		{
			for (int col = 0; col < GRID_COLS; col++)
			{
				int idx = (row * GRID_COLS + col) * 2;
				unsigned char ch = cells[idx];
				unsigned char attr = cells[idx + 1];
				if (ch == 0)
					ch = ' ';
				if ((int)attr != last_attr)
				{
					set_attr(attr);
					last_attr = (int)attr;
				}
				font_.PutChar(surf_, col * fw_, row * fh_, (char)ch);
			}
		}

		if (exited)
		{
			/* Status banner over the bottom row — the shell is gone. */
			static const char msg[] = " [process exited - press any key to close]";
			int by = (GRID_ROWS - 1) * fh_;
			surf_.ogFillRect(0, by, GRID_COLS * fw_ - 1, by + fh_ - 1, 0x00701010u);
			font_.SetFGColor(0xFF, 0xFF, 0xFF, 255);
			font_.SetBGColor(0x70, 0x10, 0x10, 255);
			font_.PutString(surf_, 0, by, msg);
			return;
		}

		/* Block cursor: filled cell with the underlying glyph inverted. */
		if (cx >= 0 && cx < GRID_COLS && cy >= 0 && cy < GRID_ROWS)
		{
			int px = cx * fw_;
			int py = cy * fh_;
			surf_.ogFillRect(px, py, px + fw_ - 1, py + fh_ - 1, 0x00AAAAAAu);
			unsigned char ch = cells[(cy * GRID_COLS + cx) * 2];
			if (ch == 0)
				ch = ' ';
			font_.SetFGColor(0, 0, 0, 255);
			font_.SetBGColor(0xAA, 0xAA, 0xAA, 255);
			font_.PutChar(surf_, px, py, (char)ch);
		}
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

	auto send_flip = [&]()
	{
		mpi_message_t m = {};
		struct display_flip *fl = (struct display_flip *)m.data;
		m.header = DISPLAY_FLIP;
		fl->window_id = win_id;
		fl->dirty_x = 0;
		fl->dirty_y = 0;
		fl->dirty_w = act_w;
		fl->dirty_h = act_h;
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

	unsigned char grid[CELL_BYTES];
	unsigned char prev[CELL_BYTES];
	std::memset(prev, 0xFF, sizeof(prev)); /* force first draw */
	bool shell_exited = false;

	for (;;)
	{
		unsigned short cx16 = 0, cy16 = 0;
		bool dirty = false;

		/* Pull the latest rendered screen; redraw only when it changed. */
		if (pty.snapshot(grid, &cx16, &cy16) == 0)
		{
			if (std::memcmp(grid, prev, sizeof(grid)) != 0)
			{
				std::memcpy(prev, grid, sizeof(grid));
				dirty = true;
			}
		}

		/* Detect the shell exiting: mark the window and draw the banner once. */
		if (!shell_exited && pty.exited())
		{
			shell_exited = true;
			set_title("Terminal (exited)");
			dirty = true;
		}

		while (mbox.try_fetch(reply))
		{
			if (reply.header == DISPLAY_CLOSE)
			{
				close_term();
				return 0;
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
		int cx = linear % GRID_COLS;
		int cy = linear / GRID_COLS;

		if (dirty)
		{
			tv.render(grid, cx, cy, shell_exited);
			send_flip();
		}

		ubix::yield();
	}

	return 0;
}
