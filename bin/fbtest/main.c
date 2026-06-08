/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * fbtest — aarch64 desktop-platform smoke test.  Exercises the new framebuffer
 * + input syscalls end to end without objGFX or the compositor:
 *
 *   sys_mapfb (43)    map the virtio-gpu framebuffer into this process
 *   sys_fbpresent(54) present a composited frame
 *   sys_getkbd (46)   cooked keystrokes from virtio-input
 *   sys_getmouse(44)  cooked relative mouse motion + buttons
 *
 * Draws a fixed test pattern (distinct from the kernel boot gradient so a
 * screendump proves userland writes reach the scanout), then runs a cursor that
 * tracks the mouse and logs every input event to the serial console.  Quits on
 * 'q'.
 */

#include <stdio.h>
#include <stdint.h>
#include <sched.h>
#include <sys/ubix_syscall.h>

struct fb_info
{
	void *base;
	uint32_t width;
	uint32_t height;
	uint16_t pitch;
	uint8_t bpp;
};

struct mouse_event
{
	int16_t dx;
	int16_t dy;
	uint8_t buttons;
};

struct kbd_event
{
	uint32_t keycode;
	uint8_t pressed;
};

UBIX_NATIVE_THUNK(_sys_mapfb, 43);
UBIX_NATIVE_THUNK(_sys_getmouse, 44);
UBIX_NATIVE_THUNK(_sys_getkbd, 46);
UBIX_NATIVE_THUNK(_sys_fbpresent, 54);

extern int _sys_mapfb(struct fb_info *info);
extern int _sys_getmouse(struct mouse_event *ev);
extern int _sys_getkbd(struct kbd_event *ev);
extern int _sys_fbpresent(void);

static struct fb_info g_fb;

static inline uint32_t *pixel_at(int x, int y)
{
	return (uint32_t *)((uint8_t *)g_fb.base + (uint32_t)y * g_fb.pitch) + x;
}

static void fill_rect(int x0, int y0, int w, int h, uint32_t color)
{
	for (int y = y0; y < y0 + h; y++)
	{
		if (y < 0 || (uint32_t)y >= g_fb.height)
			continue;
		for (int x = x0; x < x0 + w; x++)
		{
			if (x < 0 || (uint32_t)x >= g_fb.width)
				continue;
			*pixel_at(x, y) = color;
		}
	}
}

/* Paint the static test pattern: dark-blue field, a red box with a white
 * border, and a green diagonal — unmistakable against the boot gradient. */
static void draw_pattern(void)
{
	uint32_t w = g_fb.width, h = g_fb.height;

	fill_rect(0, 0, (int)w, (int)h, 0x00102040); /* dark blue */
	fill_rect((int)w / 4, (int)h / 4, (int)w / 2, (int)h / 2, 0x00FFFFFF);
	fill_rect((int)w / 4 + 8, (int)h / 4 + 8, (int)w / 2 - 16, (int)h / 2 - 16, 0x00C00020);
	for (uint32_t i = 0; i < w && i < h; i++)
		*pixel_at((int)i, (int)i) = 0x0000FF00; /* green diagonal */
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	if (_sys_mapfb(&g_fb) != 0)
	{
		printf("fbtest: sys_mapfb failed\n");
		return 1;
	}
	printf("fbtest: fb %ux%u pitch=%u bpp=%u base=%p\n", g_fb.width, g_fb.height, g_fb.pitch, g_fb.bpp, g_fb.base);

	draw_pattern();
	_sys_fbpresent();
	printf("fbtest: pattern presented — move the mouse / press keys (q quits)\n");

	int cx = (int)g_fb.width / 2, cy = (int)g_fb.height / 2;
	for (;;)
	{
		struct kbd_event kev;
		while (_sys_getkbd(&kev) == 0)
		{
			printf("fbtest: KBD keycode=0x%x pressed=%u\n", kev.keycode, kev.pressed);
			if (kev.pressed && kev.keycode == 'q')
			{
				printf("fbtest: quit\n");
				return 0;
			}
		}

		struct mouse_event mev;
		int moved = 0;
		while (_sys_getmouse(&mev) == 0)
		{
			cx += mev.dx;
			cy += mev.dy;
			if (cx < 0)
				cx = 0;
			if (cy < 0)
				cy = 0;
			if ((uint32_t)cx >= g_fb.width)
				cx = (int)g_fb.width - 1;
			if ((uint32_t)cy >= g_fb.height)
				cy = (int)g_fb.height - 1;
			moved = 1;
			printf("fbtest: MOUSE dx=%d dy=%d buttons=0x%x -> (%d,%d)\n", mev.dx, mev.dy, mev.buttons, cx, cy);
		}

		if (moved)
		{
			draw_pattern();
			fill_rect(cx - 6, cy - 6, 12, 12, 0x00FFFF00); /* yellow cursor */
			_sys_fbpresent();
		}

		sched_yield();
	}

	return 0;
}
