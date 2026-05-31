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
 * settings — the UbixOS Settings window, modelled on macOS System Settings:
 * a category sidebar on the left, the selected category's content on the right.
 * It opens on General.  Panes are built-in modules; the Desktop pane sets the
 * wallpaper, stored in ubistry and applied live by the compositor.
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

#define WIN_W 520
#define WIN_H 320
#define BG 0x00202830u         /* content background */
#define SIDEBAR_BG 0x00181E26u /* sidebar background */
#define ROW_SEL 0x00405890u    /* selected row highlight */
#define SIDEBAR_W 128
#define ROW_H 24
#define SIDE_TOP 8
#define CONTENT_X (SIDEBAR_W + 14)
#define CONTENT_TOP 12
#define FONT_PATH "/var/fonts/ROM8X8.DPF"

/* Sidebar categories (index == pane id); Settings opens on General. */
static const char *g_pane_labels[] = {"General", "Desktop"};
#define PANE_GENERAL 0
#define PANE_DESKTOP 1
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

/* y of the first wallpaper row within the Desktop pane content. */
static int desktop_rows_top()
{
	return CONTENT_TOP + 20;
}

static void draw_sidebar(ogSurface &surf, ogBitFont &font, int active)
{
	surf.ogFillRect(0, 0, SIDEBAR_W - 1, WIN_H - 1, SIDEBAR_BG);
	for (int i = 0; i < NUM_PANES; i++)
	{
		int y = SIDE_TOP + i * ROW_H;
		uint32_t bg = (i == active) ? ROW_SEL : SIDEBAR_BG;
		surf.ogFillRect(4, y, SIDEBAR_W - 5, y + ROW_H - 2, bg);
		set_color(font, 0x00F0F0F0, bg);
		font.PutString(surf, 14, y + 6, g_pane_labels[i]);
	}
}

static void draw_content(ogSurface &surf, ogBitFont &font, int active)
{
	surf.ogFillRect(SIDEBAR_W, 0, WIN_W - 1, WIN_H - 1, BG);

	set_color(font, 0x00FFFFFF, BG);
	font.PutString(surf, CONTENT_X, CONTENT_TOP, g_pane_labels[active]);

	if (active == PANE_DESKTOP)
	{
		for (int i = 0; i < (int)g_wallpapers.size(); i++)
		{
			int y = desktop_rows_top() + i * ROW_H;
			uint32_t bg = (i == g_wp_current) ? ROW_SEL : BG;
			surf.ogFillRect(CONTENT_X - 4, y, WIN_W - 9, y + ROW_H - 2, bg);
			set_color(font, 0x00F0F0F0, bg);
			font.PutString(surf, CONTENT_X + 4, y + 6, g_wallpapers[i].label.c_str());
		}
	}
	else /* General — placeholder for now */
	{
		set_color(font, 0x00A0B0C0, BG);
		font.PutString(surf, CONTENT_X, CONTENT_TOP + 28, "UbixOS desktop settings.");
		font.PutString(surf, CONTENT_X, CONTENT_TOP + 44, "Choose a category on the left.");
	}
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

	int active = PANE_GENERAL; /* open on General */

	auto render = [&]()
	{
		draw_sidebar(surf, font, active);
		draw_content(surf, font, active);
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

			/* Sidebar: switch category. */
			if (me->x < SIDEBAR_W)
			{
				if (me->y >= SIDE_TOP)
				{
					int i = (me->y - SIDE_TOP) / ROW_H;
					if (i >= 0 && i < NUM_PANES && i != active)
					{
						active = i;
						render();
					}
				}
				continue;
			}

			/* Content: dispatch to the active pane. */
			if (active == PANE_DESKTOP && me->y >= desktop_rows_top())
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
