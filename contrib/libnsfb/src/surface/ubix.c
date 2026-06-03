/*
 * uBixOS surface backend for libnsfb.
 *
 * Acts as a "views" graphical app: claims a window from the compositor over
 * MPI, points libnsfb's framebuffer at the window's shared 32bpp (0x00RRGGBB =
 * XRGB8888) buffer, flips dirty rectangles on update, and translates the
 * compositor's key/mouse/resize/close messages into nsfb events.
 *
 * (C) 2002-2026 The uBixOS Project.  Backend authored for uBixOS; libnsfb
 * itself is MIT (NetSurf).
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libnsfb.h"
#include "libnsfb_plot.h"
#include "libnsfb_event.h"

#include "nsfb.h"
#include "surface.h"
#include "plot.h"

#include <sys/mpi.h>
#include <sys/kbd.h>
#include <sys/mouse.h>
#include <views/display_proto.h>

#define UBIX_MBOX "netsurf"

static uint32_t g_win_id;
static uint8_t g_btn_state; /* last mouse button mask, for press/release edges */

/**
 * Default geometry/format before the window is claimed.
 */
static int ubix_defaults(nsfb_t *nsfb)
{
	nsfb->width = 800;
	nsfb->height = 600;
	nsfb->format = NSFB_FMT_XRGB8888;
	nsfb->bpp = 32;
	select_plotters(nsfb);
	return 0;
}

/**
 * Claim a compositor window and point the framebuffer at its shared buffer.
 */
static int ubix_initialise(nsfb_t *nsfb)
{
	mpi_message_t msg, reply;
	struct display_claim_req *req;
	struct display_ack *ack;

	if (nsfb->ptr != NULL)
		return 0; /* already claimed */

	mpi_createMbox(UBIX_MBOX);

	memset(&msg, 0, sizeof(msg));
	msg.header = DISPLAY_CLAIM;
	req = (struct display_claim_req *)msg.data;
	req->x = 40;
	req->y = 40;
	req->w = nsfb->width;
	req->h = nsfb->height;
	req->sender_pid = getpid();
	strncpy(req->title, "NetSurf", sizeof(req->title) - 1);
	strncpy(req->reply, UBIX_MBOX, sizeof(req->reply) - 1);

	while (mpi_postMessage("views", DISPLAY_CLAIM, &msg) != 0)
		usleep(5000);
	while (mpi_fetchMessage(UBIX_MBOX, &reply) != 0)
		usleep(1000);
	if (reply.header != DISPLAY_ACK)
		return -1;

	ack = (struct display_ack *)reply.data;
	g_win_id = ack->window_id;
	nsfb->width = ack->w;
	nsfb->height = ack->h;
	nsfb->bpp = 32;
	nsfb->ptr = (uint8_t *)ack->shm_base;
	nsfb->linelen = ack->pitch;
	select_plotters(nsfb);
	return 0;
}

/**
 * Set requested geometry before the window is claimed (no-op afterwards).
 */
static int ubix_set_geometry(nsfb_t *nsfb, int width, int height, enum nsfb_format_e format)
{
	if (nsfb->ptr != NULL)
		return -1; /* window already claimed; size is fixed by the compositor */
	if (width > 0)
		nsfb->width = width;
	if (height > 0)
		nsfb->height = height;
	nsfb->format = (format == NSFB_FMT_ANY) ? NSFB_FMT_XRGB8888 : format;
	nsfb->bpp = 32;
	select_plotters(nsfb);
	return 0;
}

/**
 * Release the window back to the compositor.
 */
static int ubix_finalise(nsfb_t *nsfb)
{
	mpi_message_t msg;
	struct display_release *rel;

	if (nsfb->ptr == NULL)
		return 0;
	memset(&msg, 0, sizeof(msg));
	msg.header = DISPLAY_RELEASE;
	rel = (struct display_release *)msg.data;
	rel->window_id = g_win_id;
	mpi_postMessage("views", DISPLAY_RELEASE, &msg);
	nsfb->ptr = NULL;
	return 0;
}

/**
 * Present a dirty rectangle to the compositor.
 */
static int ubix_update(nsfb_t *nsfb, nsfb_bbox_t *box)
{
	mpi_message_t msg;
	struct display_flip *fl;

	memset(&msg, 0, sizeof(msg));
	msg.header = DISPLAY_FLIP;
	fl = (struct display_flip *)msg.data;
	fl->window_id = g_win_id;
	fl->dirty_x = box ? box->x0 : 0;
	fl->dirty_y = box ? box->y0 : 0;
	fl->dirty_w = box ? (box->x1 - box->x0) : nsfb->width;
	fl->dirty_h = box ? (box->y1 - box->y0) : nsfb->height;
	mpi_postMessage("views", DISPLAY_FLIP, &msg);
	return 0;
}

/**
 * Map a uBixOS keycode to an nsfb keycode (ASCII passes through).
 */
static enum nsfb_key_code_e map_key(uint32_t kc)
{
	switch (kc)
	{
		case KEY_UP:
			return NSFB_KEY_UP;
		case KEY_DOWN:
			return NSFB_KEY_DOWN;
		case KEY_LEFT:
			return NSFB_KEY_LEFT;
		case KEY_RIGHT:
			return NSFB_KEY_RIGHT;
		case KEY_PGUP:
			return NSFB_KEY_PAGEUP;
		case KEY_PGDN:
			return NSFB_KEY_PAGEDOWN;
		default:
			if (kc < 256)
				return (enum nsfb_key_code_e)kc; /* printable/ASCII */
			return NSFB_KEY_UNKNOWN;
	}
}

/**
 * Poll the compositor mailbox and translate one message into an nsfb event.
 * timeout: 0 = non-blocking, <0 = wait forever, >0 = milliseconds.
 */
static bool ubix_input(nsfb_t *nsfb, nsfb_event_t *event, int timeout)
{
	mpi_message_t msg;
	int waited = 0;

	for (;;)
	{
		if (mpi_fetchMessage(UBIX_MBOX, &msg) == 0)
		{
			switch (msg.header)
			{
				case DISPLAY_KEY:
				{
					struct display_key *k = (struct display_key *)msg.data;
					event->type = k->pressed ? NSFB_EVENT_KEY_DOWN : NSFB_EVENT_KEY_UP;
					event->value.keycode = map_key(k->keycode);
					return true;
				}
				case DISPLAY_MOUSE:
				{
					struct display_mouse_ev *m = (struct display_mouse_ev *)msg.data;
					uint8_t changed = (uint8_t)(m->buttons ^ g_btn_state);

					/* Emit a button press/release edge, else a move. */
					if (changed & MOUSE_BTN_LEFT)
					{
						event->type = (m->buttons & MOUSE_BTN_LEFT) ? NSFB_EVENT_KEY_DOWN
						                                            : NSFB_EVENT_KEY_UP;
						event->value.keycode = NSFB_KEY_MOUSE_1;
						g_btn_state = m->buttons;
						return true;
					}
					if (changed & MOUSE_BTN_RIGHT)
					{
						event->type = (m->buttons & MOUSE_BTN_RIGHT) ? NSFB_EVENT_KEY_DOWN
						                                             : NSFB_EVENT_KEY_UP;
						event->value.keycode = NSFB_KEY_MOUSE_3;
						g_btn_state = m->buttons;
						return true;
					}
					g_btn_state = m->buttons;
					event->type = NSFB_EVENT_MOVE_ABSOLUTE;
					event->value.vector.x = m->x;
					event->value.vector.y = m->y;
					event->value.vector.z = 0;
					return true;
				}
				case DISPLAY_WINRESIZE:
				{
					struct display_winresize *wr = (struct display_winresize *)msg.data;
					nsfb->width = wr->w;
					nsfb->height = wr->h;
					nsfb->ptr = (uint8_t *)wr->shm_base;
					nsfb->linelen = wr->pitch;
					select_plotters(nsfb);
					event->type = NSFB_EVENT_RESIZE;
					event->value.resize.w = wr->w;
					event->value.resize.h = wr->h;
					return true;
				}
				case DISPLAY_CLOSE:
					event->type = NSFB_EVENT_CONTROL;
					event->value.controlcode = NSFB_CONTROL_QUIT;
					return true;
				default:
					continue; /* ignore, keep draining */
			}
		}

		if (timeout == 0)
		{
			event->type = NSFB_EVENT_NONE;
			return false;
		}
		usleep(1000);
		if (timeout > 0 && ++waited >= timeout)
		{
			event->type = NSFB_EVENT_CONTROL;
			event->value.controlcode = NSFB_CONTROL_TIMEOUT;
			return true;
		}
	}
}

const nsfb_surface_rtns_t ubix_rtns = {
    .defaults = ubix_defaults,
    .initialise = ubix_initialise,
    .finalise = ubix_finalise,
    .input = ubix_input,
    .geometry = ubix_set_geometry,
    .update = ubix_update,
};

NSFB_SURFACE_DEF(ubix, NSFB_SURFACE_UBIX, &ubix_rtns)
