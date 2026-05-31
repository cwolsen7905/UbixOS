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
 * settings — the UbixOS Settings window, modelled on macOS System Settings: a
 * category sidebar on the left, the selected category's content on the right.
 * Opens on General.  The Desktop pane sets the desktop background (an image,
 * a solid colour, or coloured jailbars) via ubistry; the compositor applies it
 * live.  Colours are chosen with a simple click-to-set RGB picker.
 */

#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <ubix/mailbox.hh>
#include <ubix/sched.hh>
#include <views/display.hh>
#include <objgfx/objgfx.h>
#include <objgfx/ogFont.h>
#include <objgfx/ogImage.h>
#include <objgfx/ogPixelFmt.h>
#include <ubistry/ubistry.h>
#include <api/netcfg.h>
#include <api/display.h>
#include <sys/kbd.h>

extern char **environ; /* session env, forwarded to launched helpers */

#define WIN_W 520
#define WIN_H 360
#define BG 0x00202830u
#define SIDEBAR_BG 0x00181E26u
#define ROW_SEL 0x00405890u
#define SIDEBAR_W 128
#define ROW_H 24
#define SIDE_TOP 8
#define CONTENT_X (SIDEBAR_W + 14)
#define CONTENT_TOP 12
#define FONT_PATH "/var/fonts/ROM8X8.DPF"

/* Desktop pane geometry. */
#define TAB_Y 40
#define TAB_W 78
#define TAB_H 22
#define SUB_TOP (TAB_Y + TAB_H + 14)
#define PICK_X (CONTENT_X + 16)
#define PICK_W 200
#define PICK_ROW 28
#define PREVIEW_X (PICK_X + PICK_W + 14)

/* Image dropdown: a combo box at SUB_TOP; the option list opens below it. */
#define DD_X (CONTENT_X - 4)
#define DD_W (WIN_W - 9 - DD_X)

/* Wallpaper preview thumbnail, shown below the dropdown when the list is closed. */
#define THUMB_W 200
#define THUMB_H 150
#define THUMB_X DD_X
#define THUMB_Y (SUB_TOP + ROW_H + 14)

/* Apply button below the thumbnail (image mode commits on Apply, not on select). */
#define APPLY_X THUMB_X
#define APPLY_Y (THUMB_Y + THUMB_H + 12)
#define APPLY_W 90
#define APPLY_H 24

/* Sidebar categories (index == pane id); Settings opens on General. */
static const char *g_pane_labels[] = {"General", "Desktop", "Appearance", "Network", "Display"};
#define PANE_GENERAL 0
#define PANE_DESKTOP 1
#define PANE_APPEARANCE 2
#define PANE_NETWORK 3
#define PANE_DISPLAY 4
#define NUM_PANES ((int)(sizeof(g_pane_labels) / sizeof(g_pane_labels[0])))

/* Display pane: a 2-column grid of enumerated resolutions. */
#define DISP_CW 125
#define DISP_CH 22
#define DISP_COLS 2

/* Network pane geometry + state (system-wide; written to bare /net keys). */
#define NM_DHCP 0
#define NM_STATIC 1
#define NET_FX (CONTENT_X + 86)
#define NET_FW 150
#define NET_RH (ROW_H + 8)
#define NET_APPLY_Y (SUB_TOP + 4 * NET_RH + 8)

/* Desktop background modes (== /views/desktop/mode). */
#define DM_IMAGE 0
#define DM_SOLID 1
#define DM_BARS 2
static const char *g_mode_labels[] = {"Image", "Solid", "Jailbars"};
static const char *g_mode_keys[] = {"image", "solid", "jailbars"};

struct ImgOption
{
	std::string label;
	std::string path;
};

static std::vector<ImgOption> g_images;
static int g_mode = DM_BARS;
static int g_img_sel = 0;
static bool g_img_open = false; /* image dropdown expanded? */

/* Cached downscaled preview of g_images[g_img_sel], rebuilt on selection change. */
static std::vector<uint32_t> g_thumb;
static int g_thumb_for = -1;
static bool g_thumb_ok = false;
static int g_solid[3] = {0x2C, 0x60, 0xA8};
static int g_bar[3] = {0x1A, 0x1A, 0x2E};
static int g_accent[3] = {0x28, 0x48, 0x70}; /* window title-bar accent */

/* Network pane state: dotted-quad strings, editable in static mode. */
static int g_net_mode = NM_DHCP;
static char g_net_field[4][24]; /* ip, netmask, gateway, dns */
static int g_net_focus = -1;    /* index of the field being edited, or -1 */
static const char *g_net_flabels[4] = {"IP Address", "Netmask", "Gateway", "DNS"};
static const char *g_net_fkeys[4] = {"/net/ip", "/net/netmask", "/net/gateway", "/net/dns"};

/* Display pane state: enumerated VBE modes (from the kernel cache). */
static struct vesa_mode g_disp_modes[32];
static int g_disp_count = 0;
static int g_disp_sel = 0;

/* Current window height — fixed width (WIN_W), variable height via resize grip. */
static int g_win_h = WIN_H;

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

static uint32_t packed(const int *rgb)
{
	return ((uint32_t)rgb[0] << 16) | ((uint32_t)rgb[1] << 8) | (uint32_t)rgb[2];
}

/*
 * Launcher — spawns helper programs (e.g. /bin/netcfg) without the parent ever
 * fork()ing after its window's shared memory exists.  A helper child is forked
 * once at startup (before the window is claimed); later launches just pipe the
 * path to that helper, which does the fork()+execve().  Mirrors the taskbar.
 */
class Launcher
{
	int fd_ = -1;

      public:
	void init()
	{
		int pfd[2];
		if (pipe(pfd) != 0)
			return;
		if (fork() == 0)
		{
			close(pfd[1]);
			char path[256];
			int len = 0;
			char ch;
			for (;;)
			{
				int r;
				do
				{
					r = (int)read(pfd[0], &ch, 1);
				} while (r < 0);
				if (r == 0)
					_exit(0);
				if (ch != '\0')
				{
					if (len < (int)sizeof(path) - 1)
						path[len++] = ch;
					continue;
				}
				if (len == 0)
					continue;
				path[len] = '\0';
				len = 0;
				if (fork() == 0)
				{
					char *argv[] = {path, nullptr};
					execve(path, argv, environ);
					_exit(1);
				}
			}
		}
		close(pfd[0]);
		fd_ = pfd[1];
	}

	void launch(const char *path)
	{
		if (fd_ >= 0)
			(void)write(fd_, path, std::strlen(path) + 1);
	}
};

static Launcher g_launcher;

/**
 * The user whose desktop this Settings instance edits.  Changes are written as
 * per-user overrides under /users/<name>; reads resolve user-first then fall
 * back to the system default.  NULL (no $USER) edits the system layer.
 */
static const char *current_user()
{
	const char *u = getenv("USER");
	return (u != NULL && u[0] != '\0') ? u : NULL;
}

/**
 * Read the Desktop pane state (image list, mode, colours) from the registry.
 */
static void load_desktop()
{
	const char *u = current_user();
	char names[UB_NAMES_MAX];

	g_images.clear();
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
				g_images.push_back({basename_of(path), path});
		}
	}

	char mode[32];
	g_mode = DM_BARS;
	if (ubistry_get_for(u, "views/desktop/mode", mode, sizeof(mode)) == 0)
	{
		if (strcmp(mode, "image") == 0)
			g_mode = DM_IMAGE;
		else if (strcmp(mode, "solid") == 0)
			g_mode = DM_SOLID;
	}

	char img[128];
	g_img_sel = 0;
	if (ubistry_get_for(u, "views/desktop/image", img, sizeof(img)) == 0)
		for (int i = 0; i < (int)g_images.size(); i++)
			if (g_images[i].path == img)
			{
				g_img_sel = i;
				break;
			}

	int v;
	if (ubistry_get_for_int(u, "views/desktop/color", &v) == 0)
	{
		g_solid[0] = (v >> 16) & 0xFF;
		g_solid[1] = (v >> 8) & 0xFF;
		g_solid[2] = v & 0xFF;
	}
	if (ubistry_get_for_int(u, "views/desktop/barcolor", &v) == 0)
	{
		g_bar[0] = (v >> 16) & 0xFF;
		g_bar[1] = (v >> 8) & 0xFF;
		g_bar[2] = v & 0xFF;
	}
	if (ubistry_get_for_int(u, "views/theme/accent", &v) == 0)
	{
		g_accent[0] = (v >> 16) & 0xFF;
		g_accent[1] = (v >> 8) & 0xFF;
		g_accent[2] = v & 0xFF;
	}
}

static void refresh_desktop()
{
	mpi_message_t r = {};
	r.header = DISPLAY_REFRESH_DESKTOP;
	ubix::post_message("views", DISPLAY_REFRESH_DESKTOP, r);
}

/* Write the current mode + its parameter as the user's override and apply it. */
static void apply()
{
	const char *u = current_user();

	ubistry_set_user(u, "views/desktop/mode", g_mode_keys[g_mode]);
	if (g_mode == DM_IMAGE && !g_images.empty())
		ubistry_set_user(u, "views/desktop/image", g_images[g_img_sel].path.c_str());
	else if (g_mode == DM_SOLID)
		ubistry_set_user_int(u, "views/desktop/color", (int)packed(g_solid));
	else if (g_mode == DM_BARS)
		ubistry_set_user_int(u, "views/desktop/barcolor", (int)packed(g_bar));
	refresh_desktop();
}

/* Write the accent colour as the user's override and notify views + taskbar. */
static void apply_accent()
{
	ubistry_set_user_int(current_user(), "views/theme/accent", (int)packed(g_accent));
	refresh_desktop(); /* views re-reads the accent for window title bars */

	mpi_message_t r = {};
	r.header = DISPLAY_THEME;
	ubix::post_message("taskbar", DISPLAY_THEME, r); /* taskbar re-reads its palette */
}

/**
 * Draw a click-to-set RGB picker editing rgb[3], with a colour preview.
 */
static void draw_picker(ogSurface &surf, ogBitFont &font, const int *rgb)
{
	static const char *chan = "RGB";
	for (int k = 0; k < 3; k++)
	{
		int by = SUB_TOP + k * PICK_ROW;
		set_color(font, 0x00C0C0C0, BG);
		char lbl[2] = {chan[k], '\0'};
		font.PutString(surf, CONTENT_X, by + 3, lbl);

		/* Gradient bar (0..255 of this channel), then the value marker. */
		for (int dx = 0; dx < PICK_W; dx++)
		{
			int v = dx * 255 / (PICK_W - 1);
			uint32_t c = (k == 0) ? ((uint32_t)v << 16) : (k == 1) ? ((uint32_t)v << 8) : (uint32_t)v;
			surf.ogFillRect(PICK_X + dx, by, PICK_X + dx, by + 13, c);
		}
		int mx = PICK_X + rgb[k] * (PICK_W - 1) / 255;
		surf.ogFillRect(mx - 1, by - 2, mx + 1, by + 15, 0x00FFFFFFu);
	}

	uint32_t col = packed(rgb);
	surf.ogFillRect(PREVIEW_X, SUB_TOP, PREVIEW_X + 28, SUB_TOP + 3 * PICK_ROW - 14, col);
	surf.ogRect(PREVIEW_X, SUB_TOP, PREVIEW_X + 28, SUB_TOP + 3 * PICK_ROW - 14, 0x00808080u);
}

/**
 * Decode the selected wallpaper and downscale it (nearest-neighbour) into the
 * THUMB_W x THUMB_H preview cache.  Rebuilt only when the selection changes.
 */
static void build_thumb()
{
	g_thumb_for = g_img_sel;
	g_thumb_ok = false;
	g_thumb.assign(THUMB_W * THUMB_H, 0);
	if (g_images.empty())
		return;

	ogImage img;
	ogSurface src;
	if (!img.Load(g_images[g_img_sel].path.c_str(), src))
		return;

	int sw = (int)src.ogGetMaxX() + 1;
	int sh = (int)src.ogGetMaxY() + 1;
	if (sw <= 0 || sh <= 0)
		return;

	for (int ty = 0; ty < THUMB_H; ty++)
		for (int tx = 0; tx < THUMB_W; tx++)
		{
			uint8_t r, g, b;
			src.ogUnpack(src.ogGetPixel(tx * sw / THUMB_W, ty * sh / THUMB_H), r, g, b);
			g_thumb[ty * THUMB_W + tx] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
		}
	g_thumb_ok = true;
}

/**
 * Paint the cached wallpaper preview (a "Preview" label + framed thumbnail).
 */
static void draw_thumb(ogSurface &surf, ogBitFont &font)
{
	if (g_thumb_for != g_img_sel)
		build_thumb();

	set_color(font, 0x00A0B0C0, BG);
	font.PutString(surf, THUMB_X, THUMB_Y - 13, "Preview");

	if (!g_thumb_ok)
	{
		surf.ogFillRect(THUMB_X, THUMB_Y, THUMB_X + THUMB_W - 1, THUMB_Y + THUMB_H - 1, 0x00181E26u);
		surf.ogRect(THUMB_X, THUMB_Y, THUMB_X + THUMB_W - 1, THUMB_Y + THUMB_H - 1, 0x00586470u);
		return;
	}

	for (int ty = 0; ty < THUMB_H; ty++)
		for (int tx = 0; tx < THUMB_W; tx++)
		{
			uint32_t c = g_thumb[ty * THUMB_W + tx];
			surf.ogSetPixel(
			    THUMB_X + tx, THUMB_Y + ty, (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c, 255);
		}
	surf.ogRect(THUMB_X, THUMB_Y, THUMB_X + THUMB_W - 1, THUMB_Y + THUMB_H - 1, 0x00586470u);
}

/**
 * Draw the wallpaper picker as a dropdown: a combo box showing the current
 * selection, plus (when expanded) the full option list below it.  This keeps the
 * pane compact no matter how many wallpapers ship.
 */
static void draw_dropdown(ogSurface &surf, ogBitFont &font)
{
	const char *cur = g_images.empty() ? "(none)" : g_images[g_img_sel].label.c_str();

	surf.ogFillRect(DD_X, SUB_TOP, DD_X + DD_W, SUB_TOP + ROW_H - 2, 0x00303A46u);
	surf.ogRect(DD_X, SUB_TOP, DD_X + DD_W, SUB_TOP + ROW_H - 2, 0x00586470u);
	set_color(font, 0x00F0F0F0, 0x00303A46u);
	font.PutString(surf, DD_X + 6, SUB_TOP + 6, cur);
	font.PutString(surf, DD_X + DD_W - 14, SUB_TOP + 6, g_img_open ? "^" : "v");

	if (!g_img_open)
		return;

	for (int i = 0; i < (int)g_images.size(); i++)
	{
		int y = SUB_TOP + ROW_H + i * ROW_H;
		uint32_t bg = (i == g_img_sel) ? ROW_SEL : 0x00283038u;
		surf.ogFillRect(DD_X, y, DD_X + DD_W, y + ROW_H - 1, bg);
		surf.ogRect(DD_X, y, DD_X + DD_W, y + ROW_H - 1, 0x00404A56u);
		set_color(font, 0x00F0F0F0, bg);
		font.PutString(surf, DD_X + 6, y + 6, g_images[i].label.c_str());
	}
}

/* ---------------------------- Network pane ---------------------------- */

/* Read the /net keys into the pane state (system-wide config; bare paths). */
static void load_network()
{
	char buf[32];

	g_net_mode = NM_DHCP;
	if (ubistry_get_str("/net/mode", buf, sizeof(buf)) == 0 && strcmp(buf, "static") == 0)
		g_net_mode = NM_STATIC;
	for (int i = 0; i < 4; i++)
	{
		g_net_field[i][0] = '\0';
		if (ubistry_get_str(g_net_fkeys[i], buf, sizeof(buf)) == 0)
			snprintf(g_net_field[i], sizeof(g_net_field[i]), "%s", buf);
	}
}

/* Write the /net keys and run netcfg to push the config into the kernel. */
static void apply_network()
{
	ubistry_set_str("/net/mode", g_net_mode == NM_STATIC ? "static" : "dhcp");
	if (g_net_mode == NM_STATIC)
		for (int i = 0; i < 4; i++)
			if (g_net_field[i][0] != '\0')
				ubistry_set_str(g_net_fkeys[i], g_net_field[i]);
	g_launcher.launch("/bin/netcfg");
}

static void draw_network(ogSurface &surf, ogBitFont &font)
{
	static const char *modes[2] = {"DHCP", "Static"};

	for (int m = 0; m < 2; m++)
	{
		int tx = CONTENT_X + m * (TAB_W + 4);
		uint32_t bg = (m == g_net_mode) ? ROW_SEL : 0x00303A46u;
		surf.ogFillRect(tx, TAB_Y, tx + TAB_W - 1, TAB_Y + TAB_H - 1, bg);
		surf.ogRect(tx, TAB_Y, tx + TAB_W - 1, TAB_Y + TAB_H - 1, 0x00586470u);
		set_color(font, 0x00F0F0F0, bg);
		font.PutString(surf, tx + 12, TAB_Y + 7, modes[m]);
	}

	if (g_net_mode == NM_STATIC)
	{
		for (int i = 0; i < 4; i++)
		{
			int y = SUB_TOP + i * NET_RH;
			bool focus = (i == g_net_focus);
			set_color(font, 0x00C0C8D0, BG);
			font.PutString(surf, CONTENT_X, y + 6, g_net_flabels[i]);
			uint32_t box = focus ? 0x00304060u : 0x00181E26u;
			surf.ogFillRect(NET_FX, y, NET_FX + NET_FW - 1, y + ROW_H - 2, box);
			surf.ogRect(NET_FX, y, NET_FX + NET_FW - 1, y + ROW_H - 2, focus ? 0x0090B0E0u : 0x00586470u);
			set_color(font, 0x00FFFFFF, box);
			char shown[28];
			snprintf(shown, sizeof(shown), focus ? "%s_" : "%s", g_net_field[i]);
			font.PutString(surf, NET_FX + 5, y + 6, shown);
		}
	}
	else
	{
		set_color(font, 0x00A0B0C0, BG);
		font.PutString(surf, CONTENT_X, SUB_TOP + 6, "Address obtained automatically via DHCP.");
	}

	surf.ogFillRect(CONTENT_X, NET_APPLY_Y, CONTENT_X + 89, NET_APPLY_Y + ROW_H - 1, ROW_SEL);
	surf.ogRect(CONTENT_X, NET_APPLY_Y, CONTENT_X + 89, NET_APPLY_Y + ROW_H - 1, 0x00586470u);
	set_color(font, 0x00FFFFFF, ROW_SEL);
	font.PutString(surf, CONTENT_X + 26, NET_APPLY_Y + 8, "Apply");
}

/* Handle a click in the Network pane; returns true if anything changed. */
static bool network_click(int x, int y)
{
	if (y >= TAB_Y && y < TAB_Y + TAB_H)
	{
		for (int m = 0; m < 2; m++)
		{
			int tx = CONTENT_X + m * (TAB_W + 4);
			if (x >= tx && x < tx + TAB_W)
			{
				g_net_mode = m;
				g_net_focus = -1;
				return true;
			}
		}
		return false;
	}
	if (g_net_mode == NM_STATIC)
	{
		for (int i = 0; i < 4; i++)
		{
			int fy = SUB_TOP + i * NET_RH;
			if (x >= NET_FX && x < NET_FX + NET_FW && y >= fy && y < fy + ROW_H)
			{
				g_net_focus = i;
				return true;
			}
		}
	}
	if (x >= CONTENT_X && x < CONTENT_X + 90 && y >= NET_APPLY_Y && y < NET_APPLY_Y + ROW_H)
	{
		g_net_focus = -1;
		apply_network();
		return true;
	}
	return false;
}

/* Edit the focused Network field: digits/dot type, backspace deletes, Tab
 * moves to the next field, Enter/Esc finish editing. */
static bool network_key(uint32_t kc)
{
	if (g_net_mode != NM_STATIC || g_net_focus < 0)
		return false;

	char *f = g_net_field[g_net_focus];
	int len = (int)strlen(f);

	if ((kc >= '0' && kc <= '9') || kc == '.')
	{
		if (len < (int)sizeof(g_net_field[0]) - 1)
		{
			f[len] = (char)kc;
			f[len + 1] = '\0';
		}
		return true;
	}
	if (kc == 0x08 || kc == 0x7F)
	{
		if (len > 0)
			f[len - 1] = '\0';
		return true;
	}
	if (kc == '\t')
	{
		g_net_focus = (g_net_focus + 1) % 4;
		return true;
	}
	if (kc == '\r' || kc == '\n' || kc == KEY_ESC)
	{
		g_net_focus = -1;
		return true;
	}
	return false;
}

/* ---------------------------- Display pane ---------------------------- */

/* Read the enumerated VBE modes and select the saved /display/mode. */
static void load_display()
{
	char mode_s[16];
	unsigned cur = 0x118;

	g_disp_count = ubix_vesa_modes(g_disp_modes, 32);
	if (g_disp_count < 0)
		g_disp_count = 0;
	if (ubistry_get_str("/display/mode", mode_s, sizeof(mode_s)) == 0)
		cur = (unsigned)strtol(mode_s, NULL, 0);

	g_disp_sel = 0;
	for (int i = 0; i < g_disp_count; i++)
		if (g_disp_modes[i].mode == cur)
		{
			g_disp_sel = i;
			break;
		}
}

/* Persist the chosen mode and ask the compositor to switch to it live. */
static void apply_display()
{
	if (g_disp_count <= 0)
		return;

	char buf[16];
	snprintf(buf, sizeof(buf), "0x%X", g_disp_modes[g_disp_sel].mode);
	ubistry_set_str("/display/mode", buf);

	mpi_message_t m = {};
	struct display_setmode *sm = (struct display_setmode *)m.data;
	m.header = DISPLAY_SETMODE;
	sm->mode = g_disp_modes[g_disp_sel].mode;
	ubix::post_message("views", DISPLAY_SETMODE, m);
}

static void draw_display(ogSurface &surf, ogBitFont &font)
{
	set_color(font, 0x00A0B0C0, BG);
	font.PutString(surf, CONTENT_X, CONTENT_TOP + 22, "Screen resolution");

	if (g_disp_count <= 0)
	{
		font.PutString(surf, CONTENT_X, SUB_TOP + 6, "No video modes reported.");
		return;
	}

	for (int i = 0; i < g_disp_count; i++)
	{
		int r = i / DISP_COLS, c = i % DISP_COLS;
		int x = CONTENT_X + c * DISP_CW;
		int y = SUB_TOP + r * DISP_CH;
		uint32_t bg = (i == g_disp_sel) ? ROW_SEL : 0x00303A46u;
		surf.ogFillRect(x, y, x + DISP_CW - 4, y + DISP_CH - 3, bg);
		surf.ogRect(x, y, x + DISP_CW - 4, y + DISP_CH - 3, 0x00586470u);
		set_color(font, 0x00F0F0F0, bg);
		char lbl[24];
		snprintf(lbl, sizeof(lbl), "%ux%u", g_disp_modes[i].width, g_disp_modes[i].height);
		font.PutString(surf, x + 8, y + 5, lbl);
	}

	int rows = (g_disp_count + DISP_COLS - 1) / DISP_COLS;
	int ay = SUB_TOP + rows * DISP_CH + 8;
	surf.ogFillRect(CONTENT_X, ay, CONTENT_X + 89, ay + ROW_H - 1, ROW_SEL);
	surf.ogRect(CONTENT_X, ay, CONTENT_X + 89, ay + ROW_H - 1, 0x00586470u);
	set_color(font, 0x00FFFFFF, ROW_SEL);
	font.PutString(surf, CONTENT_X + 26, ay + 8, "Apply");
}

/* Handle a click in the Display pane (resolution grid + Apply). */
static bool display_click(int x, int y)
{
	for (int i = 0; i < g_disp_count; i++)
	{
		int r = i / DISP_COLS, c = i % DISP_COLS;
		int cx = CONTENT_X + c * DISP_CW;
		int cy = SUB_TOP + r * DISP_CH;
		if (x >= cx && x < cx + DISP_CW - 4 && y >= cy && y < cy + DISP_CH - 3)
		{
			g_disp_sel = i;
			return true;
		}
	}

	int rows = (g_disp_count + DISP_COLS - 1) / DISP_COLS;
	int ay = SUB_TOP + rows * DISP_CH + 8;
	if (g_disp_count > 0 && x >= CONTENT_X && x < CONTENT_X + 90 && y >= ay && y < ay + ROW_H)
	{
		apply_display();
		return true;
	}
	return false;
}

static void draw_content(ogSurface &surf, ogBitFont &font, int pane)
{
	surf.ogFillRect(SIDEBAR_W, 0, WIN_W - 1, g_win_h - 1, BG);
	set_color(font, 0x00FFFFFF, BG);
	font.PutString(surf, CONTENT_X, CONTENT_TOP, g_pane_labels[pane]);

	if (pane == PANE_APPEARANCE)
	{
		set_color(font, 0x00A0B0C0, BG);
		font.PutString(surf, CONTENT_X, CONTENT_TOP + 22, "Accent colour (window title bars)");
		draw_picker(surf, font, g_accent);
		return;
	}

	if (pane == PANE_NETWORK)
	{
		draw_network(surf, font);
		return;
	}

	if (pane == PANE_DISPLAY)
	{
		draw_display(surf, font);
		return;
	}

	if (pane != PANE_DESKTOP)
	{
		set_color(font, 0x00A0B0C0, BG);
		font.PutString(surf, CONTENT_X, CONTENT_TOP + 28, "UbixOS desktop settings.");
		font.PutString(surf, CONTENT_X, CONTENT_TOP + 44, "Choose a category on the left.");
		return;
	}

	/* Mode tabs. */
	for (int m = 0; m < 3; m++)
	{
		int tx = CONTENT_X + m * (TAB_W + 4);
		uint32_t bg = (m == g_mode) ? ROW_SEL : 0x00303A46u;
		surf.ogFillRect(tx, TAB_Y, tx + TAB_W - 1, TAB_Y + TAB_H - 1, bg);
		surf.ogRect(tx, TAB_Y, tx + TAB_W - 1, TAB_Y + TAB_H - 1, 0x00586470u);
		set_color(font, 0x00F0F0F0, bg);
		font.PutString(surf, tx + 8, TAB_Y + 7, g_mode_labels[m]);
	}

	if (g_mode == DM_IMAGE)
	{
		draw_dropdown(surf, font);
		if (!g_img_open)
		{
			draw_thumb(surf, font);
			surf.ogFillRect(APPLY_X, APPLY_Y, APPLY_X + APPLY_W - 1, APPLY_Y + APPLY_H - 1, ROW_SEL);
			surf.ogRect(APPLY_X, APPLY_Y, APPLY_X + APPLY_W - 1, APPLY_Y + APPLY_H - 1, 0x00586470u);
			set_color(font, 0x00FFFFFF, ROW_SEL);
			font.PutString(surf, APPLY_X + 26, APPLY_Y + 8, "Apply");
		}
	}
	else
	{
		draw_picker(surf, font, g_mode == DM_SOLID ? g_solid : g_bar);
	}
}

/* Handle a click in the Desktop pane content; returns true if it changed state. */
static bool desktop_click(int x, int y)
{
	/* Mode tabs. */
	if (y >= TAB_Y && y < TAB_Y + TAB_H)
	{
		for (int m = 0; m < 3; m++)
		{
			int tx = CONTENT_X + m * (TAB_W + 4);
			if (x >= tx && x < tx + TAB_W)
			{
				g_mode = m;
				g_img_open = false;
				/* Image mode commits via Apply; colour modes apply live. */
				if (m != DM_IMAGE)
					apply();
				return true;
			}
		}
		return false;
	}

	if (g_mode == DM_IMAGE)
	{
		bool in_box = (x >= DD_X && x <= DD_X + DD_W && y >= SUB_TOP && y < SUB_TOP + ROW_H);

		if (!g_img_open)
		{
			if (in_box)
			{
				g_img_open = true;
				return true;
			}
			/* Apply button commits the selected wallpaper. */
			if (x >= APPLY_X && x < APPLY_X + APPLY_W && y >= APPLY_Y && y < APPLY_Y + APPLY_H)
			{
				apply();
				return true;
			}
			return false;
		}

		/* Open: clicking the header closes; a row only selects (preview); else dismiss. */
		if (in_box)
		{
			g_img_open = false;
			return true;
		}
		int i = (y - (SUB_TOP + ROW_H)) / ROW_H;
		if (y >= SUB_TOP + ROW_H && x >= DD_X && x <= DD_X + DD_W && i >= 0 && i < (int)g_images.size())
		{
			g_img_sel = i;
			g_img_open = false;
			return true; /* preview updates; commit happens on Apply */
		}
		g_img_open = false;
		return true;
	}

	/* RGB picker: a click on a channel bar sets that channel. */
	int *rgb = (g_mode == DM_SOLID) ? g_solid : g_bar;
	for (int k = 0; k < 3; k++)
	{
		int by = SUB_TOP + k * PICK_ROW;
		if (y >= by - 2 && y <= by + 15 && x >= PICK_X && x < PICK_X + PICK_W)
		{
			int v = (x - PICK_X) * 255 / (PICK_W - 1);
			if (v < 0)
				v = 0;
			if (v > 255)
				v = 255;
			rgb[k] = v;
			apply();
			return true;
		}
	}
	return false;
}

/* Handle a click in the Appearance pane (the accent RGB picker). */
static bool appearance_click(int x, int y)
{
	for (int k = 0; k < 3; k++)
	{
		int by = SUB_TOP + k * PICK_ROW;
		if (y >= by - 2 && y <= by + 15 && x >= PICK_X && x < PICK_X + PICK_W)
		{
			int v = (x - PICK_X) * 255 / (PICK_W - 1);
			if (v < 0)
				v = 0;
			if (v > 255)
				v = 255;
			g_accent[k] = v;
			apply_accent();
			return true;
		}
	}
	return false;
}

/*
 * Keyboard navigation for the image dropdown.  Up/Down move the selection (the
 * preview updates but the desktop is not changed until Apply); Space toggles the
 * list; Enter applies when the list is closed (or closes it when open); Esc
 * closes it.  Returns true if anything changed.
 */
static bool desktop_key(uint32_t kc)
{
	if (g_mode != DM_IMAGE || g_images.empty())
		return false;

	int n = (int)g_images.size();
	switch (kc)
	{
		case KEY_UP:
			if (g_img_sel > 0)
			{
				g_img_sel--;
				return true; /* preview only */
			}
			return false;
		case KEY_DOWN:
			if (g_img_sel < n - 1)
			{
				g_img_sel++;
				return true; /* preview only */
			}
			return false;
		case ' ':
			g_img_open = !g_img_open;
			return true;
		case '\r':
		case '\n':
			if (g_img_open)
				g_img_open = false; /* confirm selection into the box */
			else
				apply(); /* commit the previewed wallpaper */
			return true;
		case KEY_ESC:
			if (g_img_open)
			{
				g_img_open = false;
				return true;
			}
			return false;
		default:
			break;
	}
	return false;
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

	load_desktop();
	load_network();
	load_display();

	/* Fork the launcher helper now, BEFORE the window's shared memory exists,
	 * so a later fork() (to run netcfg) can't COW-break the shared region. */
	g_launcher.init();

	mpi_message_t msg = {};
	struct display_claim_req *creq = (struct display_claim_req *)msg.data;
	msg.header = DISPLAY_CLAIM;
	creq->x = 60;
	creq->y = 60;
	creq->w = WIN_W;
	creq->h = WIN_H;
	/* Fixed width, variable height (the macOS System Settings style). */
	creq->min_w = WIN_W;
	creq->max_w = WIN_W;
	creq->min_h = 280;
	creq->max_h = 720;
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

	int active = PANE_GENERAL;

	auto draw_sidebar = [&]()
	{
		surf.ogFillRect(0, 0, SIDEBAR_W - 1, g_win_h - 1, SIDEBAR_BG);
		for (int i = 0; i < NUM_PANES; i++)
		{
			int y = SIDE_TOP + i * ROW_H;
			uint32_t bg = (i == active) ? ROW_SEL : SIDEBAR_BG;
			surf.ogFillRect(4, y, SIDEBAR_W - 5, y + ROW_H - 2, bg);
			set_color(font, 0x00F0F0F0, bg);
			font.PutString(surf, 14, y + 6, g_pane_labels[i]);
		}
	};

	auto render = [&]()
	{
		draw_sidebar();
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
			if (reply.header == DISPLAY_WINRESIZE)
			{
				struct display_winresize *wr = (struct display_winresize *)reply.data;
				act_w = wr->w;
				act_h = wr->h;
				g_win_h = wr->h;
				surf.ogAttach(wr->shm_base, (uint32_t)wr->w, (uint32_t)wr->h, OG_PIXFMT_32BPP);
				render();
				continue;
			}
			if (reply.header == DISPLAY_KEY)
			{
				struct display_key *dk = (struct display_key *)reply.data;
				if (dk->pressed)
				{
					bool changed = false;
					if (active == PANE_DESKTOP)
						changed = desktop_key(dk->keycode);
					else if (active == PANE_NETWORK)
						changed = network_key(dk->keycode);
					if (changed)
						render();
				}
				continue;
			}
			if (reply.header != DISPLAY_MOUSE)
				continue;

			struct display_mouse_ev *me = (struct display_mouse_ev *)reply.data;
			if (me->buttons & 1)
				continue; /* act on release */

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
			}
			else if (active == PANE_DESKTOP)
			{
				if (desktop_click(me->x, me->y))
					render();
			}
			else if (active == PANE_APPEARANCE)
			{
				if (appearance_click(me->x, me->y))
					render();
			}
			else if (active == PANE_NETWORK)
			{
				if (network_click(me->x, me->y))
					render();
			}
			else if (active == PANE_DISPLAY)
			{
				if (display_click(me->x, me->y))
					render();
			}
		}
		ubix::yield();
	}

	return 0;
}
