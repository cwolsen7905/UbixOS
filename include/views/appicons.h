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
 * views/appicons.h — which icon represents an app, and how to draw it.
 *
 * Shared by the taskbar (task items, launcher, recents) and the compositor
 * (title-bar icons) so an app looks the same everywhere.  Apps the design's
 * icon set has an app-* picture for use it; others get a fallback glyph in one
 * of the design's app colours.  In light mode the app-* glyphs (baked in
 * dark-mode colours) are drawn tinted with the light palette's app colour.
 */

#pragma once

#include <cstring>
#include <string>
#include <views/uidraw.h>

/* What to draw for an app icon at a requested size. */
struct ui_app_icon_spec
{
	const char *name; /* icon in /usr/share/icons/<size>/ */
	int size;         /* size to load (may differ from the requested size) */
	int dx, dy;       /* offset from the requested top-left */
	bool tint;        /* draw as a mask in `color` (else its own colours) */
	uint32_t color;
	bool disc; /* 48px: draw the round base (edge + surface) first */
};

/**
 * The design's app-* icon for an app, matched from its label / exec / window
 * title (case-insensitive); "" if the set has no dedicated picture.
 */
static inline const char *ui_app_icon_name(const std::string &text)
{
	std::string t;
	for (char c : text)
		t += (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
	static const struct
	{
		const char *key, *icon;
	} map[] = {
	    {"term", "app-terminal"},
	    {"file", "app-files"},
	    {"setting", "app-settings"},
	    {"netsurf", "app-browser"},
	    {"browser", "app-browser"},
	    {"edit", "app-editor"},
	    {"calc", "app-calculator"},
	    {"music", "app-music"},
	    {"mp3", "app-music"},
	    {"photo", "app-photos"},
	    {"image", "app-photos"},
	    {"note", "app-notes"},
	    {"mail", "app-mail"},
	    {"calendar", "app-calendar"},
	};
	for (auto &m : map)
		if (t.find(m.key) != std::string::npos)
			return m.icon;
	return "";
}

/**
 * Identity colour for an app-* glyph: the design's per-app colour; Settings /
 * Calculator are neutral, Files / Packages use the accent.
 */
static inline uint32_t ui_app_tone(const char *icon, const struct ui_theme &t)
{
	static const struct
	{
		const char *icon;
		int color; /* ui_app_color; -1 accent, -2 neutral */
	} tones[] = {
	    {"app-terminal", UI_APP_GREEN},
	    {"app-browser", UI_APP_CYAN},
	    {"app-mail", UI_APP_ORANGE},
	    {"app-music", UI_APP_PINK},
	    {"app-photos", UI_APP_GOLD},
	    {"app-calendar", UI_APP_RED},
	    {"app-notes", UI_APP_YELLOW},
	    {"app-editor", UI_APP_BLUE},
	    {"app-files", -1},
	    {"app-packages", -1},
	    {"app-settings", -2},
	    {"app-calculator", -2},
	};
	for (auto &e : tones)
		if (std::strcmp(icon, e.icon) == 0)
			return e.color == -1 ? t.accent_400 : (e.color == -2 ? t.text_muted : t.app[e.color]);
	return t.text_muted;
}

/**
 * Decide how to draw an app's icon at size (16 or 48) for theme t.
 */
static inline ui_app_icon_spec ui_app_icon(const std::string &label,
                                           const std::string &exec,
                                           int size,
                                           const struct ui_theme &t)
{
	ui_app_icon_spec s = {"grid", size, 0, 0, false, 0, false};
	const char *ic = ui_app_icon_name(label + " " + exec);
	if (ic[0] != '\0')
	{
		uint32_t tone = ui_app_tone(ic, t);
		s.name = ic;
		if (t.mode != UI_THEME_LIGHT)
		{
			/* dark: the pre-rendered icons; neutral 16px glyphs follow the theme */
			if (size < 48 &&
			    (std::strcmp(ic, "app-settings") == 0 || std::strcmp(ic, "app-calculator") == 0))
				s.tint = true, s.color = tone;
		}
		else if (size >= 48)
			s.size = 24, s.dx = s.dy = 12, s.tint = true, s.color = tone, s.disc = true;
		else
			s.tint = true, s.color = tone;
		return s;
	}

	/* No dedicated picture: a design glyph in an app colour. */
	std::string l;
	for (char c : label)
		l += (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
	const char *g = "grid";
	int col = -1;
	if (l.find("disk") != std::string::npos)
		g = "drive", col = UI_APP_ORANGE;
	else if (l.find("activity") != std::string::npos || l.find("monitor") != std::string::npos)
		g = "list", col = UI_APP_CYAN;
	else if (l.find("doom") != std::string::npos)
		g = "monitor", col = UI_APP_RED;
	else if (l.find("tessera") != std::string::npos)
		g = "grid", col = UI_APP_YELLOW;
	else if (l.find("about") != std::string::npos)
		g = "ubix-mark";
	bool mark = std::strcmp(g, "ubix-mark") == 0; /* has its own colours */
	s.name = g;
	s.tint = !mark;
	s.color = col < 0 ? t.accent_400 : t.app[col];
	if (size >= 48)
		s.size = 24, s.dx = s.dy = 12, s.disc = true;
	else if (mark)
		s.size = 24, s.dx = s.dy = -4;
	else
		s.size = 16;
	return s;
}

/**
 * Draw an app's icon (top-left x,y; size 16 or 48) on a ui_canvas.
 */
static inline void ui_draw_app_icon(
    ui_canvas &cv, const std::string &label, const std::string &exec, int size, int x, int y, const struct ui_theme &t)
{
	ui_app_icon_spec s = ui_app_icon(label, exec, size, t);
	if (s.disc)
		ui_outlined_round(cv, x, y, 48, 48, 24, t.edge, t.surface);
	const ui_icon &ic = ui_icon_get(s.size, s.name);
	if (s.tint)
		ui_draw_icon_tint(cv, ic, x + s.dx, y + s.dy, s.color);
	else
		ui_draw_icon(cv, ic, x + s.dx, y + s.dy);
}
