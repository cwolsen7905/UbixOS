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
 * views/theme.h — the system UI palette (ubixos-v2 design).
 *
 * Source of truth for the values: docs/design/mockups/ubixos-v2/tokens.css
 * (dark) and the light palette in the design canvas.  Colours are packed
 * 0x00RRGGBB; translucent fills carry a separate alpha byte (0..255).
 *
 * Appearance is an OS-wide *signal*, not a mandate:
 *   - views/theme/mode   "dark" | "light"   (ubistry, per-user override)
 *   - views/theme/accent 0xRRGGBB as int     (ubistry, per-user override)
 *   - a DISPLAY_THEME message is broadcast when either changes.
 * Apps that follow the system call ui_theme_load() at start-up and again on
 * DISPLAY_THEME; apps with their own look (the terminal is always dark) may
 * ignore the mode or read a fixed palette with ui_theme_get().
 *
 * With no accent override the designed accent ramp is used verbatim; a custom
 * accent derives its ramp by blending toward the ground (tints) and toward
 * white/black (text steps), so the accent stays "lines and marks, never floods".
 */

#pragma once

#include <stdint.h>
#include <string.h>
#include <ubistry/ubistry.h>

#ifdef __cplusplus
extern "C"
{
#endif

	enum ui_theme_mode
	{
		UI_THEME_DARK = 0,
		UI_THEME_LIGHT = 1,
	};

	struct ui_theme
	{
		enum ui_theme_mode mode;

		/* Ground & surfaces */
		uint32_t ground;      /* window body, login card, panels */
		uint32_t surface;     /* active title bar, sidebars, inputs on panels */
		uint32_t raised;      /* hover on surface */
		uint32_t edge;        /* 1px outlines on windows, fields, cards */
		uint32_t edge_strong; /* hovered outlines, active window edge */
		uint32_t console;     /* terminal background (always dark) */

		/* Text */
		uint32_t text;
		uint32_t text_muted; /* secondary text, inactive titles */
		uint32_t text_faint; /* captions, placeholders */
		uint32_t glyph;      /* monochrome icon tint (window buttons, tray) */

		/* Neutral steps used for hover / indicators */
		uint32_t neutral_600; /* running-task mark */
		uint32_t neutral_700; /* scrollbar thumb hover */
		uint32_t neutral_800; /* active task item, button hover, scrollbar thumb */

		/* Accent ramp (single accent) */
		uint32_t accent;     /* lines, outlines, focus ring, indicators */
		uint32_t accent_200; /* text on accent tints */
		uint32_t accent_300; /* accent-coloured body text */
		uint32_t accent_400; /* accent icons, pressed outline, caret */
		uint32_t accent_700; /* pressed fill */
		uint32_t accent_800; /* pressed / strong selection */
		uint32_t accent_900; /* hover tint, selection fill */

		/* Danger (close button only) */
		uint32_t danger;
		uint32_t danger_hover;
		uint32_t danger_pressed;

		/* Translucent fills: colour + alpha (alpha-blended, never blurred) */
		uint32_t taskbar;
		uint8_t taskbar_a;
		uint32_t panel; /* launcher, popovers */
		uint8_t panel_a;
		uint32_t divider; /* hairlines inside windows/bars */
		uint8_t divider_a;
		uint32_t scrim; /* wallpaper dim behind the login card */
		uint8_t scrim_a;

		/* Shadows: one offset shadow per element (y offset, reach, peak alpha /256) */
		uint32_t shadow; /* shadow colour */
		uint8_t shadow_win_y, shadow_win_reach, shadow_win_a;
		uint8_t shadow_inact_y, shadow_inact_reach, shadow_inact_a;
		uint8_t shadow_panel_y, shadow_panel_reach, shadow_panel_a;
		uint8_t shadow_bar_y, shadow_bar_reach, shadow_bar_a;

		uint32_t neutral_400; /* secondary label beside primary text (tray date) */

		/* App identity colours (icons, launcher): green, cyan, orange, pink,
		 * yellow, blue, red, gold — darker in light mode for contrast. */
		uint32_t app[8];
	};

/* Fixed metrics (px) — identical in both modes. */
#define UI_TASKBAR_H 40
#define UI_TASKBAR_INSET 8
/* Screen rows kept clear of maximized windows: bar + its inset + the same gap
 * above it (1280x800 -> windows maximize to 1280x744). */
#define UI_TASKBAR_RESERVE (UI_TASKBAR_H + 2 * UI_TASKBAR_INSET)
/* Screen width below which the bar is edge-to-edge (no inset) — 800x480 etc. */
#define UI_COMPACT_W 1000
/* Rows kept clear of maximized windows for a given screen width. */
#define ui_taskbar_reserve(screen_w) ((screen_w) < UI_COMPACT_W ? UI_TASKBAR_H : UI_TASKBAR_RESERVE)
#define UI_TITLEBAR_H 32
#define UI_WINBTN 22    /* circular window-control button */
#define UI_WINCAPS_H 28 /* window-control capsule height */
#define UI_CONTROL_H 32 /* buttons, fields */
#define UI_ROW_H 28     /* list rows */
#define UI_SCROLLBAR_W 8
#define UI_R_SM 4
#define UI_R_MD 6
#define UI_R_LG 10 /* windows */
#define UI_R_XL 12 /* taskbar, launcher, login card */
#define UI_R_PILL 16
#define UI_FS_UI 12 /* default UI text */
#define UI_FS_SMALL 11
#define UI_FS_TERM 13

/* Designed default accents (dark / light). */
#define UI_ACCENT_DARK 0x009184D9u
#define UI_ACCENT_LIGHT 0x00796CBFu

	/* Indexes into ui_theme.app[]. */
	enum ui_app_color
	{
		UI_APP_GREEN,
		UI_APP_CYAN,
		UI_APP_ORANGE,
		UI_APP_PINK,
		UI_APP_YELLOW,
		UI_APP_BLUE,
		UI_APP_RED,
		UI_APP_GOLD
	};

	/* clang-format off */
static const struct ui_theme ui_theme_dark = {
	UI_THEME_DARK,
	0x161826, 0x232532, 0x292B31, 0x3F424D, 0x595D6C, 0x0F1019,
	0xE9E9ED, 0x9397AB, 0x75798C, 0xCFD3E5,
	0x75798C, 0x595D6C, 0x3F424D,
	UI_ACCENT_DARK, 0xE7E5FE, 0xD2CEFD, 0xB5ABFC, 0x5D5294, 0x423A6A, 0x2B2741,
	0xD0485E, 0x5A2330, 0x7A2D3D,
	0x161826, 0xEB, 0x161826, 0xF5, 0xE9E9ED, 0x1F, 0x080616, 0x9E,
	0x000000,
	8, 12, 140,
	4, 8, 102,
	6, 10, 128,
	4, 8, 115,
	0xB2B6CA,
	{0x6FD3A0, 0x7AD7FF, 0xFFB36B, 0xFF7AD9, 0xE9D36B, 0x9FB4FF, 0xF0788C, 0xF0C36B},
};

static const struct ui_theme ui_theme_light = {
	UI_THEME_LIGHT,
	0xF3F5FE, 0xE4E7F5, 0xD9DCEC, 0xCFD3E5, 0xB2B6CA, 0x0F1019,
	0x292B31, 0x595D6C, 0x75798C, 0x3F424D,
	0x9397AB, 0xB2B6CA, 0xCFD3E5,
	UI_ACCENT_LIGHT, 0x423A6A, 0x5D5294, 0x796CBF, 0xB5ABFC, 0xD2CEFD, 0xE7E5FE,
	0xC23A52, 0xF6D9DE, 0xEEBCC5,
	0xF3F5FE, 0xEB, 0xF3F5FE, 0xF5, 0x292B31, 0x24, 0x080616, 0x9E,
	0x1E1C46,
	8, 14, 56,
	4, 8, 36,
	6, 10, 36,
	4, 8, 40,
	0x75798C,
	{0x1F8A5A, 0x0E7BA8, 0xB8630F, 0xB0328F, 0x8A7400, 0x3F5FC8, 0xC23A52, 0x9A6C00},
};
	/* clang-format on */

	/**
	 * Linear blend of two packed 0xRRGGBB colours: t = 0 gives a, t = 256 gives b.
	 */
	static inline uint32_t ui_mix(uint32_t a, uint32_t b, uint32_t t)
	{
		uint32_t r = (((a >> 16) & 0xFF) * (256 - t) + ((b >> 16) & 0xFF) * t) >> 8;
		uint32_t g = (((a >> 8) & 0xFF) * (256 - t) + ((b >> 8) & 0xFF) * t) >> 8;
		uint32_t bl = ((a & 0xFF) * (256 - t) + (b & 0xFF) * t) >> 8;
		return (r << 16) | (g << 8) | bl;
	}

	/**
	 * Return the built-in palette for a mode with the designed accent ramp.
	 */
	static inline struct ui_theme ui_theme_get(enum ui_theme_mode mode)
	{
		return mode == UI_THEME_LIGHT ? ui_theme_light : ui_theme_dark;
	}

	/**
	 * Replace the accent ramp with one derived from a custom accent colour.
	 *
	 * Tints (700/800/900) blend the accent into the ground so selection fills stay
	 * quiet; text steps (200/300/400) blend toward white on dark and toward the
	 * text colour on light so accent text stays readable.  The designed accent of
	 * the theme's own mode is left untouched (its hand-tuned ramp is kept).
	 */
	static inline void ui_theme_set_accent(struct ui_theme *t, uint32_t a)
	{
		a &= 0x00FFFFFFu;
		if (a == (t->mode == UI_THEME_LIGHT ? UI_ACCENT_LIGHT : UI_ACCENT_DARK))
			return;
		uint32_t hi = t->mode == UI_THEME_LIGHT ? t->text : 0xFFFFFFu;
		t->accent = a;
		t->accent_400 = ui_mix(a, hi, t->mode == UI_THEME_LIGHT ? 0 : 90);
		t->accent_300 = ui_mix(a, hi, t->mode == UI_THEME_LIGHT ? 64 : 154);
		t->accent_200 = ui_mix(a, hi, t->mode == UI_THEME_LIGHT ? 128 : 205);
		t->accent_700 = ui_mix(t->ground, a, t->mode == UI_THEME_LIGHT ? 115 : 148);
		t->accent_800 = ui_mix(t->ground, a, t->mode == UI_THEME_LIGHT ? 64 : 90);
		t->accent_900 = ui_mix(t->ground, a, t->mode == UI_THEME_LIGHT ? 26 : 38);
	}

	/**
	 * Resolve the session's theme from the registry: mode (views/theme/mode,
	 * default dark) and accent (views/theme/accent, default = the mode's designed
	 * accent).  user may be NULL/empty for the system default (login screen).
	 */
	static inline struct ui_theme ui_theme_load(const char *user)
	{
		const char *u = (user != NULL && user[0] != '\0') ? user : NULL;
		char mode[16];
		int accent;
		struct ui_theme t = ui_theme_dark;

		if (ubistry_get_for(u, "views/theme/mode", mode, sizeof(mode)) == 0 && strcmp(mode, "light") == 0)
			t = ui_theme_light;
		if (ubistry_get_for_int(u, "views/theme/accent", &accent) == 0)
			ui_theme_set_accent(&t, (uint32_t)accent);
		return t;
	}

#ifdef __cplusplus
}
#endif
