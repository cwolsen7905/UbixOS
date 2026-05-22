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

#include <string>
#include <vector>
#include <cstring>
#include <ubix/mailbox.hh>
#include <ubix/sched.hh>
#include <ubix/process.hh>
#include <views/display.hh>
#include <objgfx/objgfx.h>
#include <objgfx/ogFont.h>
#include <objgfx/ogPixelFmt.h>

#define TERM_W    640
#define TERM_H    400
#define TERM_BG   0x00101010u
#define TERM_FG_R 0xC0u
#define TERM_FG_G 0xC0u
#define TERM_FG_B 0xC0u
#define FONT_PATH "sys:/var/fonts/ROM8X8.DPF"

/* ------------------------------------------------------------------ */
/* TerminalView — cell grid, surface, and font; single responsibility   */
/* ------------------------------------------------------------------ */

class TerminalView {
	ogSurface              surf_;
	ogBitFont              font_;
	int                    cols_    = 0;
	int                    rows_    = 0;
	int                    cur_col_ = 0;
	int                    cur_row_ = 0;
	std::vector<std::string> lines_;

	void scroll() {
		lines_.erase(lines_.begin());
		lines_.push_back(std::string(cols_, ' '));
		cur_row_ = rows_ - 1;
	}

public:
	bool attach(void *shm, int w, int h) {
		return surf_.ogAttach(shm, (uint32_t)w, (uint32_t)h, OG_PIXFMT_32BPP);
	}

	bool load_font(const char *path) {
		return font_.Load(path, 0);
	}

	void init_grid(int w, int h) {
		cols_ = w / (int)font_.GetWidth();
		rows_ = h / (int)font_.GetHeight();
		lines_.assign(rows_, std::string(cols_, ' '));
	}

	int cols() const { return cols_; }
	int rows() const { return rows_; }

	void putchar(char c) {
		if (c == '\n' || c == '\r') {
			cur_col_ = 0;
			if (cur_row_ < rows_ - 1)
				cur_row_++;
			else
				scroll();
			return;
		}
		if (c == '\b') {
			if (cur_col_ > 0) {
				cur_col_--;
				lines_[cur_row_][cur_col_] = ' ';
			}
			return;
		}
		if (cur_col_ < cols_) {
			lines_[cur_row_][cur_col_] = c;
			cur_col_++;
		}
		if (cur_col_ >= cols_) {
			cur_col_ = 0;
			if (cur_row_ < rows_ - 1)
				cur_row_++;
			else
				scroll();
		}
	}

	void puts(const char *s) {
		while (*s)
			putchar(*s++);
	}

	void redraw(int act_w, int act_h) {
		int fw = (int)font_.GetWidth();
		int fh = (int)font_.GetHeight();

		surf_.ogFillRect(0, 0, act_w - 1, act_h - 1, TERM_BG);

		font_.SetFGColor(TERM_FG_R, TERM_FG_G, TERM_FG_B, 255);
		font_.SetBGColor(
		    (TERM_BG >> 16) & 0xFF,
		    (TERM_BG >>  8) & 0xFF,
		     TERM_BG        & 0xFF,
		    255);

		for (int r = 0; r < (int)lines_.size(); r++)
			font_.PutString(surf_, 0, r * fh, lines_[r].c_str());

		int cx = cur_col_ * fw;
		int cy = cur_row_ * fh;
		surf_.ogFillRect(cx, cy, cx + fw - 1, cy + fh - 1, 0x00C0C0C0u);
	}
};

/* ------------------------------------------------------------------ */
/* Key → ASCII                                                          */
/* ------------------------------------------------------------------ */

static char
key_to_ascii(uint32_t kc, uint8_t pressed)
{
	if (!pressed) return 0;
	if (kc >= 0x20 && kc < 0x7F) return (char)kc;
	if (kc == '\n' || kc == '\r') return '\n';
	if (kc == '\b') return '\b';
	return 0;
}

/* ------------------------------------------------------------------ */
/* main — thin event loop; wires TerminalView to shell and display      */
/* ------------------------------------------------------------------ */

int
main(int argc, char **argv)
{
	(void)argc; (void)argv;

	ubix::Mailbox mbox;
	mbox.assign("term." + std::to_string(ubix::pid()));
	if (!mbox.create())
		return 1;

	static char *tcsh_argv[] = { (char *)"tcsh", nullptr };
	static char *tcsh_envp[] = {
		(char *)"HOME=/",
		(char *)"SHELL=/bin/tcsh",
		(char *)"PATH=/bin",
		(char *)"TERM=dumb",
		nullptr
	};
	ubix::Shell shell;
	shell.spawn("sys:/bin/tcsh", tcsh_argv, tcsh_envp);

	static const char views[] = "views";

	mpi_message_t msg = {};
	struct display_claim_req *creq = (struct display_claim_req *)msg.data;
	msg.header       = DISPLAY_CLAIM;
	creq->x          = 20;
	creq->y          = 20;
	creq->w          = TERM_W;
	creq->h          = TERM_H;
	creq->sender_pid = ubix::pid();
	std::strncpy(creq->title, "Terminal", sizeof(creq->title) - 1);
	std::strncpy(creq->reply, mbox.c_str(), sizeof(creq->reply) - 1);
	ubix::post_message(views, DISPLAY_CLAIM, msg);

	mpi_message_t reply;
	while (!mbox.try_fetch(reply))
		ubix::yield();

	if (reply.header != DISPLAY_ACK)
		return 1;

	struct display_ack *da = (struct display_ack *)reply.data;
	uint32_t win_id = da->window_id;
	void    *shm    = da->shm_base;
	int      act_w  = da->w;
	int      act_h  = da->h;

	if (!shm || act_w <= 0 || act_h <= 0)
		return 1;

	TerminalView tv;
	if (!tv.attach(shm, act_w, act_h) || !tv.load_font(FONT_PATH))
		return 1;
	tv.init_grid(act_w, act_h);
	shell.set_nonblock();

	auto send_flip = [&]() {
		mpi_message_t m = {};
		struct display_flip *fl = (struct display_flip *)m.data;
		m.header      = DISPLAY_FLIP;
		fl->window_id = win_id;
		fl->dirty_x   = 0;
		fl->dirty_y   = 0;
		fl->dirty_w   = act_w;
		fl->dirty_h   = act_h;
		ubix::post_message(views, DISPLAY_FLIP, m);
	};

	tv.redraw(act_w, act_h);
	send_flip();

	int dirty = 0;

	for (;;) {
		dirty = 0;

		if (shell.valid()) {
			char outbuf[128];
			int n;
			while ((n = shell.read(outbuf, sizeof(outbuf) - 1)) > 0) {
				outbuf[n] = '\0';
				tv.puts(outbuf);
				dirty = 1;
			}
		}

		while (mbox.try_fetch(reply)) {
			if (reply.header == DISPLAY_CLOSE) {
				mpi_message_t rel = {};
				struct display_release *dr = (struct display_release *)rel.data;
				rel.header    = DISPLAY_RELEASE;
				dr->window_id = win_id;
				ubix::post_message(views, DISPLAY_RELEASE, rel);
				shell.kill();
				shell.close_fds();
				mbox.destroy();
				return 0;
			}
			if (reply.header != DISPLAY_KEY)
				continue;

			struct display_key *dk = (struct display_key *)reply.data;
			char ch = key_to_ascii(dk->keycode, dk->pressed);
			if (ch == 0)
				continue;

			/* Local echo: tcsh's stdin is a pipe so isatty(0) is
			 * false and tcsh runs non-interactively — it won't echo
			 * individual characters until a full line is received.
			 * Show the character immediately so keystrokes appear as
			 * typed rather than all at once on Enter. */
			tv.putchar(ch);
			dirty = 1;
			shell.write(&ch, 1);
		}

		if (dirty) {
			tv.redraw(act_w, act_h);
			send_flip();
		}

		ubix::yield();
	}

	return 0;
}
