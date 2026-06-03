/*-
 * Copyright (c) 2002-2026 The uBixOS Project.
 * All rights reserved.
 *
 * nsfbtest — validate the libnsfb uBixOS surface backend: claim a compositor
 * window via libnsfb, draw a few rectangles into its shared buffer through the
 * libnsfb plotters, flip, and run an event loop until ESC/close.  Proves the
 * objGFX surface backend (claim/buffer/flip/input) end-to-end before NetSurf.
 */

#include <libnsfb.h>
#include <libnsfb_plot.h>
#include <libnsfb_event.h>
#include <stdio.h>

int main(void)
{
	nsfb_t *nsfb;
	nsfb_bbox_t full, r;
	int w = 0, h = 0;
	enum nsfb_format_e fmt;

	nsfb = nsfb_new(NSFB_SURFACE_UBIX);
	if (nsfb == NULL)
	{
		printf("nsfbtest: nsfb_new failed\n");
		return (1);
	}
	nsfb_set_geometry(nsfb, 640, 480, NSFB_FMT_XRGB8888);
	if (nsfb_init(nsfb) != 0)
	{
		printf("nsfbtest: nsfb_init failed (compositor running?)\n");
		return (1);
	}
	nsfb_get_geometry(nsfb, &w, &h, &fmt);
	printf("nsfbtest: window %dx%d claimed\n", w, h);

	full.x0 = 0;
	full.y0 = 0;
	full.x1 = w;
	full.y1 = h;
	nsfb_claim(nsfb, &full);
	nsfb_plot_rectangle_fill(nsfb, &full, 0xff303040); /* background */
	r.x0 = 40;
	r.y0 = 40;
	r.x1 = w - 40;
	r.y1 = 90;
	nsfb_plot_rectangle_fill(nsfb, &r, 0xffc06030); /* bar */
	r.x0 = 60;
	r.y0 = 130;
	r.x1 = 260;
	r.y1 = 330;
	nsfb_plot_rectangle_fill(nsfb, &r, 0xff3070c0); /* box */
	nsfb_update(nsfb, &full);

	for (;;)
	{
		nsfb_event_t ev;
		if (!nsfb_event(nsfb, &ev, -1))
			continue;
		if (ev.type == NSFB_EVENT_CONTROL && ev.value.controlcode == NSFB_CONTROL_QUIT)
			break;
		if (ev.type == NSFB_EVENT_KEY_DOWN && ev.value.keycode == NSFB_KEY_ESCAPE)
			break;
	}

	nsfb_free(nsfb);
	return (0);
}
