//
// Copyright (c) 2002-2026 The UbixOS Project.
//
// UbixOS platform layer for doomgeneric.
// This file is part of the DOOM binary and is therefore distributed under
// the GNU General Public License v2 (same license as doomgeneric/DOOM).
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//

/*
 * doomgeneric_ubix.c — UbixOS platform layer for doomgeneric.
 *
 * Implements the 6-function DG_ API:
 *   DG_Init          — open framebuffer
 *   DG_DrawFrame     — scale 320×200 DOOM frame to full VESA screen
 *   DG_SleepMs       — sleep via usleep
 *   DG_GetTicksMs    — milliseconds via gettimeofday
 *   DG_GetKey        — poll keyboard and convert to DOOM key codes
 *   DG_SetWindowTitle — no-op
 *
 * Usage: doom [-iwad sys:/path/to/doom1.wad]
 */

#include "doomgeneric.h"
#include "doomkeys.h"

#include <fb/fb.h>
#include <sys/kbd.h>
#include <sys/mpi.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* Inspect DOOM internal state from the platform layer */
extern int gametic;
extern int gamestate;
extern int screenvisible;

static struct fb_info g_fb;
static uint32_t       g_start_ms;

static uint32_t
now_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint32_t)tv.tv_sec * 1000u + (uint32_t)tv.tv_usec / 1000u;
}

/* ------------------------------------------------------------------ */
/* DG_ platform functions                                               */
/* ------------------------------------------------------------------ */

/*
 * DG_Init — map the VESA framebuffer into this process.
 *
 * sys_mapfb requires the kernel's VESA driver to be initialized first.
 * views does this by sending MPI 0x82 to "system".  If fb_open fails
 * (VESA not yet up), we send that message ourselves and retry.
 */
void
DG_Init(void)
{
	if (fb_open(&g_fb) == 0) {
		g_start_ms = now_ms();
		return;
	}

	/* VESA not yet initialised — ask the kernel display driver to set it up */
	mpi_message_t msg, reply;
	memset(&msg, 0, sizeof(msg));
	msg.header = 0x82;
	memcpy(msg.data, "doom", 5);

	mpi_createMbox("doom");
	mpi_postMessage("system", 0x82, &msg);

	/* Block until system replies */
	while (mpi_fetchMessage("doom", &reply) != 0)
		usleep(1000);

	mpi_destroyMbox("doom");

	if (reply.data[0] == 0) {
		fprintf(stderr, "doom: VESA init failed\n");
		g_start_ms = now_ms();
		return;
	}

	/*
	 * The system task calls vesa_init() synchronously before replying,
	 * so the FB globals are already set.  fb_open should succeed on the
	 * first try; the retry loop is a safety net for slow hardware.
	 */
	int tries = 100;
	while (fb_open(&g_fb) != 0 && --tries > 0)
		usleep(10000);

	if (tries == 0 || !g_fb.base) {
		fprintf(stderr, "doom: fb_open failed after VESA init — no display\n");
		return;
	}

	g_start_ms = now_ms();

	/*
	 * Fill the framebuffer with a dark-blue loading colour.  This gives
	 * visible feedback while the 4 MB WAD is parsed.  DOOM's first
	 * DG_DrawFrame call will overwrite it.
	 */
	if (g_fb.base) {
		uint8_t  *p  = (uint8_t *)g_fb.base;
		uint32_t  sz = (uint32_t)g_fb.pitch * g_fb.height;
		uint32_t  i;
		if (g_fb.bpp == 32) {
			uint32_t *p32 = (uint32_t *)p;
			for (i = 0; i < sz / 4; i++)
				p32[i] = 0x00000040;   /* dark blue */
		} else {
			for (i = 0; i + 2 < sz; i += 3) {
				p[i]     = 0x40;       /* B */
				p[i + 1] = 0x00;       /* G */
				p[i + 2] = 0x00;       /* R = dark blue on screen */
			}
		}
	}
}

/*
 * DG_DrawFrame — blit DOOM's 320×200 ARGB buffer to the VESA framebuffer,
 * scaled to the largest integer multiple that fits, centred on screen.
 *
 * DOOM's DG_ScreenBuffer is 0x00RRGGBB.  Our framebuffer byte order is BGR:
 * byte[0]=blue, byte[1]=green, byte[2]=red.  Writing a uint32_t 0x00RRGGBB
 * on little-endian puts the bytes in the right order for both 24bpp and 32bpp.
 *
 * Mode 0x118 (triggered via MPI 0x82) is 1024×768×24bpp on QEMU/SeaBIOS.
 * DG_DrawFrame handles both 24bpp (3 bytes/pixel) and 32bpp (4 bytes/pixel).
 */
void
DG_DrawFrame(void)
{
	if (!g_fb.base)
		return;

	/*
	 * Diagnostic block.  Remove once rendering confirmed.
	 *
	 * Scans 16 pixels spread across DG_ScreenBuffer to test for content.
	 * Top-left 8×8 corner indicator:
	 *   Blinking GREEN  → DG_ScreenBuffer has non-zero content (palette issue)
	 *   Blinking RED    → DG_ScreenBuffer is all-zero   (game not rendering)
	 *   Blinking YELLOW → DG_ScreenBuffer timeout: draws test gradient instead
	 *
	 * After 70 frames (~2 s) with no content a test gradient is drawn
	 * directly to the DOOM area to confirm framebuffer addressing works.
	 */
	{
		static uint32_t diag_frame = 0;
		diag_frame++;

		/* Sample 16 pixels spread across the 320×200 buffer */
		uint32_t has_content = 0;
		for (int _s = 0; _s < 16; _s++) {
			uint32_t idx = (uint32_t)_s * (DOOMGENERIC_RESX * DOOMGENERIC_RESY / 16);
			if (DG_ScreenBuffer[idx] & 0xFFFFFF) { has_content = 1; break; }
		}

		/* Print key state every 35 frames so we can see it in the log */
		if (diag_frame % 35 == 1) {
			fprintf(stderr,
			        "DOOM diag: frame=%u gametic=%d gs=%d sv=%d "
			        "buf[0]=%08x buf[4000]=%08x\n",
			        diag_frame, gametic, gamestate, screenvisible,
			        DG_ScreenBuffer[0], DG_ScreenBuffer[4000]);
			fflush(stderr);
		}

		uint32_t diag_color;
		if (diag_frame > 70 && !has_content) {
			diag_color = (diag_frame & 1) ? 0xFFFF00 : 0x777700; /* yellow = timeout */
		} else if (has_content) {
			diag_color = (diag_frame & 1) ? 0x00FF00 : 0x007700; /* green = content */
		} else {
			diag_color = (diag_frame & 1) ? 0xFF0000 : 0x770000; /* red = empty */
		}

		uint8_t *db = (uint8_t *)g_fb.base;
		uint32_t bpp = g_fb.bpp;
		for (int _dy = 0; _dy < 8; _dy++) {
			uint8_t *row = db + (uint32_t)_dy * g_fb.pitch;
			for (int _dx = 0; _dx < 8; _dx++) {
				if (bpp == 32) {
					((uint32_t *)row)[_dx] = diag_color;
				} else {
					uint8_t *p = row + _dx * 3;
					p[0] = diag_color & 0xFF;
					p[1] = (diag_color >> 8) & 0xFF;
					p[2] = (diag_color >> 16) & 0xFF;
				}
			}
		}

		/* After 70 frames with no content, draw a test gradient over DOOM area */
		if (diag_frame > 70 && !has_content) {
			int32_t sx    = (int32_t)((g_fb.width  - DOOMGENERIC_RESX * 3) / 2);
			int32_t sy    = (int32_t)((g_fb.height - DOOMGENERIC_RESY * 3) / 2);
			for (uint32_t gy = 0; gy < (uint32_t)DOOMGENERIC_RESY * 3; gy++) {
				uint8_t *row = db + (uint32_t)(sy + (int32_t)gy) * g_fb.pitch;
				for (uint32_t gx = 0; gx < (uint32_t)DOOMGENERIC_RESX * 3; gx++) {
					uint8_t rv = (uint8_t)(gx * 255 / (DOOMGENERIC_RESX * 3));
					uint8_t gv = (uint8_t)(gy * 255 / (DOOMGENERIC_RESY * 3));
					if (bpp == 32) {
						((uint32_t *)row)[sx + (int32_t)gx] = (uint32_t)(rv << 16) | gv;
					} else {
						uint8_t *p = row + ((uint32_t)(sx) + gx) * 3;
						p[0] = 0;   /* B */
						p[1] = gv;  /* G */
						p[2] = rv;  /* R */
					}
				}
			}
			return; /* skip normal blit when showing gradient */
		}
	}

	const uint32_t *src = DG_ScreenBuffer;
	uint32_t        sw  = g_fb.width;
	uint32_t        sh  = g_fb.height;

	uint32_t sx    = sw / DOOMGENERIC_RESX;
	uint32_t sy    = sh / DOOMGENERIC_RESY;
	uint32_t scale = (sx < sy) ? sx : sy;
	if (scale < 1)
		scale = 1;

	uint32_t dw  = DOOMGENERIC_RESX * scale;
	uint32_t dh  = DOOMGENERIC_RESY * scale;
	int32_t  ox  = (int32_t)((sw - dw) / 2);
	int32_t  oy  = (int32_t)((sh - dh) / 2);

	uint8_t *base  = (uint8_t *)g_fb.base;
	uint32_t pitch = g_fb.pitch;

	for (uint32_t dy = 0; dy < dh; dy++) {
		const uint32_t *src_row  = src + (dy / scale) * DOOMGENERIC_RESX;
		uint8_t        *dst_base = base + (uint32_t)(oy + (int32_t)dy) * pitch;

		if (g_fb.bpp == 32) {
			uint32_t *dst_row = (uint32_t *)dst_base + ox;
			for (uint32_t dx = 0; dx < dw; dx++)
				dst_row[dx] = src_row[dx / scale];
		} else {
			/* 24bpp: 3 bytes per pixel, BGR byte order */
			dst_base += (uint32_t)ox * 3;
			for (uint32_t dx = 0; dx < dw; dx++) {
				uint32_t pix = src_row[dx / scale];
				dst_base[0] = pix & 0xFF;
				dst_base[1] = (pix >> 8) & 0xFF;
				dst_base[2] = (pix >> 16) & 0xFF;
				dst_base += 3;
			}
		}
	}
}

void
DG_SleepMs(uint32_t ms)
{
	usleep(ms * 1000u);
}

uint32_t
DG_GetTicksMs(void)
{
	return now_ms() - g_start_ms;
}

/*
 * Translate UbixOS kbd_event_t keycode to a doomgeneric/doomkeys.h code.
 * Returns 0 for keys DOOM doesn't care about.
 */
static unsigned char
to_doom_key(uint32_t kc)
{
	switch (kc) {
	/* Arrow keys */
	case KEY_UP:    return KEY_UPARROW;
	case KEY_DOWN:  return KEY_DOWNARROW;
	case KEY_LEFT:  return KEY_LEFTARROW;
	case KEY_RIGHT: return KEY_RIGHTARROW;

	/* Modifiers — this doomgeneric uses KEY_FIRE/KEY_USE, not KEY_RCTRL */
	case KEY_LCTRL:  return KEY_FIRE;    /* fire   (key_fire  = KEY_FIRE  = 0xa3) */
	case KEY_LALT:   return KEY_RALT;    /* strafe (key_strafe = KEY_RALT = 0xb8) */
	case KEY_LSHIFT: return KEY_RSHIFT;  /* run   (key_speed  = KEY_RSHIFT = 0xb6) */

	/* Common action keys */
	case ' ':      return KEY_USE;       /* use   (key_use = KEY_USE = 0xa2) */
	case KEY_ESC:  return KEY_ESCAPE;
	case '\r':
	case '\n':     return KEY_ENTER;
	case '\t':     return KEY_TAB;
	case 0x7f:
	case '\b':     return KEY_BACKSPACE;

	/* F-keys: KEY_F1=0x104..KEY_F12=0x10F; DOOM uses 0xbb..0xc6 */
	case KEY_F1:  return (unsigned char)(0x80 + 0x3b);
	case KEY_F2:  return (unsigned char)(0x80 + 0x3c);
	case KEY_F3:  return (unsigned char)(0x80 + 0x3d);
	case KEY_F4:  return (unsigned char)(0x80 + 0x3e);
	case KEY_F5:  return (unsigned char)(0x80 + 0x3f);
	case KEY_F6:  return (unsigned char)(0x80 + 0x40);
	case KEY_F7:  return (unsigned char)(0x80 + 0x41);
	case KEY_F8:  return (unsigned char)(0x80 + 0x42);
	case KEY_F9:  return (unsigned char)(0x80 + 0x43);
	case KEY_F10: return (unsigned char)(0x80 + 0x44);
	case KEY_F11: return (unsigned char)(0x80 + 0x57);
	case KEY_F12: return (unsigned char)(0x80 + 0x58);

	default:
		/* Pass printable ASCII straight through */
		if (kc >= 0x20 && kc < 0x80)
			return (unsigned char)kc;
		return 0;
	}
}

int
DG_GetKey(int *pressed, unsigned char *doomKey)
{
	kbd_event_t ev;
	if (fb_poll_kbd(&ev) != 0)
		return 0;
	unsigned char dk = to_doom_key(ev.keycode);
	if (dk == 0)
		return 0;
	*pressed = ev.pressed;
	*doomKey = dk;
	return 1;
}

void
DG_SetWindowTitle(const char *title)
{
	(void)title;
}

/* ------------------------------------------------------------------ */
/* Entry point                                                          */
/* ------------------------------------------------------------------ */

int
main(int argc, char **argv)
{
	/* Redirect stderr to a log file readable after exit from graphics mode */
	freopen("sys:/doom.log", "w", stderr);
	fprintf(stderr, "doom: BUILD 6 (opl2 keyon fix) started\n");
	fflush(stderr);

	/*
	 * If -iwad was not given, inject a default so the user does not need to
	 * type it.  doom1.wad lives at sys:/bin/doom1.wad on the UbixOS image.
	 */
	int has_iwad = 0;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-iwad") == 0) {
			has_iwad = 1;
			break;
		}
	}
	static char  default_iwad_path[] = "sys:/bin/doom1.wad";
	static char *new_argv[64];
	int          new_argc = argc;
	if (!has_iwad && argc < 62) {
		int i;
		for (i = 0; i < argc; i++)
			new_argv[i] = argv[i];
		new_argv[i++] = "-iwad";
		new_argv[i++] = default_iwad_path;
		new_argv[i]   = NULL;
		new_argc      = i;
		argv          = new_argv;
	}

	doomgeneric_Create(new_argc, argv);
	for (;;)
		doomgeneric_Tick();
	return 0;
}
