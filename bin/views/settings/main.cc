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
 * settings — the UbixOS Settings window, modelled on macOS System Preferences:
 * a single window showing an icon grid ("home"); clicking an icon drills into
 * that pane in-place, with a "Show All" button to return.  Panes are built-in
 * modules (v1: Desktop, which sets the wallpaper).  Pane settings are stored in
 * the ubistry registry; the Desktop pane asks the compositor to repaint live.
 */

#include <string>
#include <vector>
#include <cstring>
#include <ubix/mailbox.hh>
#include <ubix/sched.hh>
#include <views/display.hh>
#include <objgfx/objgfx.h>
#include <objgfx/ogFont.h>
#include <objgfx/ogPixelFmt.h>
#include <ubistry/ubistry.h>

#define WIN_W 480
#define WIN_H 320
#define BG 0x00202830u
#define ROW_SEL 0x00405890u
#define BACK_H 26
#define CELL_W 84
#define CELL_H 84
#define COLS 5
#define MARGIN 12
#define ICON_SZ 48
#define ROW_H 24
#define FONT_PATH "/var/fonts/ROM8X8.DPF"

/* Built-in panes shown in the home grid (index == view id). */
static const char *g_pane_labels[] = {"Desktop"};
#define PANE_DESKTOP 0
#define NUM_PANES ((int)(sizeof(g_pane_labels) / sizeof(g_pane_labels[0])))

/* Desktop-pane state: selectable wallpapers ("None" + registry entries). */
struct WpOption
{
	std::string label;
	std::string path; /* empty = None (solid desktop) */
};
static std::vector<WpOption> g_wallpapers;
static int g_wp_current = 0;

static std::string basename_of(const std::string &p)
{
	size_t s = p.find_last_of('/');
	return (s == std::string::npos) ? p : p.substr(s + 1);
}

static void set_color(ogBitFont &f, uint32_t fg, uint32_t bg)
{
	f.SetFGColor((fg >> 16) & 0xFF, (fg >> 8) & 0xFF, fg & 0xFF, 255);
	f.SetBGColor((bg >> 16) & 0xFF, (bg >> 8) & 0xFF, bg & 0xFF, 255);
}

/**
 * Load the Desktop pane's wallpaper choices and the current selection.
 */
static void load_wallpapers()
{
	char names[UB_NAMES_MAX];

	g_wallpapers.clear();
	g_wallpapers.push_back({"None (solid)", ""});
	if (ubistry_enum("/settings/wallpapers", names, sizeof(names)) > 0)
	{
		std::string ns(names);
		size_t start = 0;
		while (start < ns.size())
		{
			size_t nl = ns.find('\n', start);
			std::string child = ns.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
			start = (nl == std::string::npos) ? ns.size() : nl + 1;
			if (child.empty())
				continue;
			char path[128];
			if (ubistry_get_str(("/settings/wallpapers/" + child).c_str(), path, sizeof(path)) == 0)
				g_wallpapers.push_back({basename_of(path), path});
		}
	}

	g_wp_current = 0;
	char cur[128];
	if (ubistry_get_str("/views/desktop/wallpaper", cur, sizeof(cur)) == 0)
		for (int i = 0; i < (int)g_wallpapers.size(); i++)
			if (g_wallpapers[i].path == cur)
			{
				g_wp_current = i;
				break;
			}
}

/**
 * Draw the home view: a grid of pane icons (placeholder tiles for now).
 */
static void draw_home(ogSurface &surf, ogBitFont &font)
{
	surf.ogFillRect(0, 0, WIN_W - 1, WIN_H - 1, BG);
	for (int i = 0; i < NUM_PANES; i++)
	{
		int col = i % COLS;
		int row = i / COLS;
		int x = MARGIN + col * CELL_W;
		int y = MARGIN + row * CELL_H;
		int ix = x + (CELL_W - ICON_SZ) / 2;
		uint32_t tile = 0x00405890u;

		surf.ogFillRect(ix, y + 4, ix + ICON_SZ - 1, y + 4 + ICON_SZ - 1, tile);
		surf.ogRect(ix, y + 4, ix + ICON_SZ - 1, y + 4 + ICON_SZ - 1, 0x00708090u);
		set_color(font, 0x00FFFFFF, tile);
		char letter[2] = {g_pane_labels[i][0], '\0'};
		font.PutString(surf, ix + ICON_SZ / 2 - 4, y + 4 + ICON_SZ / 2 - 4, letter);
		set_color(font, 0x00E0E0E0, BG);
		font.PutString(surf, x + 4, y + 4 + ICON_SZ + 4, g_pane_labels[i]);
	}
}

/**
 * Draw the "Show All ‹ | <title>" back bar shared by all panes.
 */
static void draw_back_bar(ogSurface &surf, ogBitFont &font, const char *title)
{
	surf.ogFillRect(0, 0, WIN_W - 1, BACK_H - 1, 0x00161C24u);
	set_color(font, 0x0090C0F0, 0x00161C24u);
	font.PutString(surf, 8, 9, "< Show All");
	set_color(font, 0x00E0E0E0, 0x00161C24u);
	font.PutString(surf, 120, 9, title);
}

static void draw_desktop_pane(ogSurface &surf, ogBitFont &font)
{
	surf.ogFillRect(0, BACK_H, WIN_W - 1, WIN_H - 1, BG);
	draw_back_bar(surf, font, "Desktop");

	int top = BACK_H + 12;
	set_color(font, 0x00C0D0E0, BG);
	font.PutString(surf, 12, top, "Background:");

	for (int i = 0; i < (int)g_wallpapers.size(); i++)
	{
		int y = top + 20 + i * ROW_H;
		uint32_t bg = (i == g_wp_current) ? ROW_SEL : BG;
		surf.ogFillRect(8, y, WIN_W - 9, y + ROW_H - 2, bg);
		set_color(font, 0x00F0F0F0, bg);
		font.PutString(surf, 16, y + 6, g_wallpapers[i].label.c_str());
	}
}

/* y of the first wallpaper row within the Desktop pane. */
static int desktop_rows_top()
{
	return BACK_H + 12 + 20;
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	if (!ubix::views_running())
	{
		printf("settings: views compositor is not running\n");
		return 1;
	}

	ubix::Mailbox mbox;
	mbox.assign("settings." + std::to_string(ubix::pid()));
	if (!mbox.create())
		return 1;

	load_wallpapers();

	mpi_message_t msg = {};
	struct display_claim_req *creq = (struct display_claim_req *)msg.data;
	msg.header = DISPLAY_CLAIM;
	creq->x = 60;
	creq->y = 60;
	creq->w = WIN_W;
	creq->h = WIN_H;
	creq->sender_pid = ubix::pid();
	std::strncpy(creq->title, "Settings", sizeof(creq->title) - 1);
	std::strncpy(creq->reply, mbox.c_str(), sizeof(creq->reply) - 1);
	ubix::post_message("views", DISPLAY_CLAIM, msg);

	mpi_message_t reply;
	while (!mbox.try_fetch(reply))
		ubix::yield();
	if (reply.header != DISPLAY_ACK)
		return 1;

	struct display_ack *da = (struct display_ack *)reply.data;
	uint32_t win_id = da->window_id;
	void *shm = da->shm_base;
	int act_w = da->w;
	int act_h = da->h;
	if (!shm || act_w <= 0 || act_h <= 0)
		return 1;

	ogSurface surf;
	ogBitFont font;
	if (!surf.ogAttach(shm, (uint32_t)act_w, (uint32_t)act_h, OG_PIXFMT_32BPP) || !font.Load(FONT_PATH, 0))
		return 1;

	auto flip = [&]()
	{
		mpi_message_t m = {};
		struct display_flip *fl = (struct display_flip *)m.data;
		m.header = DISPLAY_FLIP;
		fl->window_id = win_id;
		fl->dirty_w = act_w;
		fl->dirty_h = act_h;
		ubix::post_message("views", DISPLAY_FLIP, m);
	};

	int view = -1; /* -1 = home, else pane id */

	auto render = [&]()
	{
		if (view < 0)
			draw_home(surf, font);
		else if (view == PANE_DESKTOP)
			draw_desktop_pane(surf, font);
		flip();
	};

	render();

	for (;;)
	{
		while (mbox.try_fetch(reply))
		{
			if (reply.header == DISPLAY_CLOSE)
			{
				mpi_message_t rel = {};
				struct display_release *dr = (struct display_release *)rel.data;
				rel.header = DISPLAY_RELEASE;
				dr->window_id = win_id;
				ubix::post_message("views", DISPLAY_RELEASE, rel);
				mbox.destroy();
				return 0;
			}
			if (reply.header != DISPLAY_MOUSE)
				continue;

			struct display_mouse_ev *me = (struct display_mouse_ev *)reply.data;
			if (me->buttons & 1)
				continue; /* act on release */

			if (view < 0)
			{
				/* Home grid: drill into the clicked pane. */
				if (me->x >= MARGIN && me->y >= MARGIN)
				{
					int col = (me->x - MARGIN) / CELL_W;
					int row = (me->y - MARGIN) / CELL_H;
					int i = row * COLS + col;
					if (col >= 0 && col < COLS && i >= 0 && i < NUM_PANES)
					{
						view = i;
						render();
					}
				}
				continue;
			}

			/* Pane view: the back bar returns home. */
			if (me->y < BACK_H)
			{
				view = -1;
				render();
				continue;
			}

			if (view == PANE_DESKTOP && me->y >= desktop_rows_top())
			{
				int i = (me->y - desktop_rows_top()) / ROW_H;
				if (i >= 0 && i < (int)g_wallpapers.size())
				{
					g_wp_current = i;
					ubistry_set_str("/views/desktop/wallpaper", g_wallpapers[i].path.c_str());
					mpi_message_t r = {};
					r.header = DISPLAY_REFRESH_DESKTOP;
					ubix::post_message("views", DISPLAY_REFRESH_DESKTOP, r);
					render();
				}
			}
		}
		ubix::yield();
	}

	return 0;
}
