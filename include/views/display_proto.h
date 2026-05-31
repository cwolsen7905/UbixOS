/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
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
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DISPLAY_PROTO_H
#define _DISPLAY_PROTO_H

#include <stdint.h>

/*
 * UbixOS display server MPI protocol.
 *
 * Design rule: MPI carries signals only — never drawing commands.
 * All pixel data moves through shared memory (Phase 6+).
 *
 * Message layout:
 *   mpi_message_t.header  = msg_type (one of the defines below)
 *   mpi_message_t.data[]  = payload struct cast from data[]
 *
 * All structs must fit within MESSAGE_LENGTH (248) bytes.
 */

/* Client → display */
#define DISPLAY_CLAIM 1            /* request a window region */
#define DISPLAY_FLIP 2             /* buffer updated, please composite */
#define DISPLAY_RELEASE 3          /* window closing */
#define DISPLAY_QUERY 8            /* request screen geometry */
#define DISPLAY_RAISE 11           /* ask compositor to raise/focus a window */
#define DISPLAY_SETTITLE 13        /* update an existing window's title bar text */
#define DISPLAY_REFRESH_DESKTOP 14 /* re-read desktop settings and repaint desktop */
#define DISPLAY_SET_USER 15        /* set the active session user (per-user desktop) */
#define DISPLAY_THEME 16           /* re-read theme (accent colour) and repaint (to taskbar) */

/* display → client */
#define DISPLAY_ACK 4     /* region granted; carries window_id + shm token */
#define DISPLAY_DENIED 5  /* request refused */
#define DISPLAY_KEY 6     /* keyboard event forwarded to focused window */
#define DISPLAY_MOUSE 7   /* mouse event forwarded to focused window */
#define DISPLAY_INFO 9    /* response to DISPLAY_QUERY */
#define DISPLAY_CLOSE 10  /* compositor closed the window (close button) */
#define DISPLAY_NOTIFY 12 /* window added or removed (sent to taskbar) */

/* Height of the server-drawn title bar (0 for no_decor windows) */
#define DECOR_H 18

/*
 * DISPLAY_CLAIM payload (client → display).
 * x,y,w,h: requested window position and size (ignored if fullscreen).
 * title:   window title string.
 * reply:   name of the client's MPI mailbox for the ACK/DENIED response.
 */
struct display_claim_req
{
	int32_t x, y, w, h;
	int32_t sender_pid; /* client's PID — needed for vmm_share_region */
	char title[64];
	char reply[64];   /* client mailbox name */
	uint8_t no_decor; /* 1 = skip server-side title bar (panels, flyouts) */
};

/*
 * DISPLAY_ACK payload (display → client).
 * window_id: opaque handle used in subsequent FLIP/RELEASE messages.
 * shm_base:  virtual address of the shared pixel buffer in client's space
 *            (populated in Phase 6; NULL until then).
 * pitch:     bytes per scanline of the shared buffer.
 * x,y,w,h:  actual region granted (may differ from request).
 */
struct display_ack
{
	uint32_t window_id;
	void *shm_base;
	uint16_t pitch;
	int32_t x, y, w, h;
};

/*
 * DISPLAY_FLIP payload (client → display).
 * window_id: handle from DISPLAY_ACK.
 * dirty_x/y/w/h: damaged rectangle within the buffer (0,0,w,h = full redraw).
 */
struct display_flip
{
	uint32_t window_id;
	int32_t dirty_x, dirty_y, dirty_w, dirty_h;
};

/*
 * DISPLAY_RELEASE payload (client → display).
 */
struct display_release
{
	uint32_t window_id;
};

/*
 * DISPLAY_CLOSE payload (display → client).
 * Sent when the user clicks the close button on the server-drawn title bar.
 * The client should clean up and exit; the compositor has already removed
 * the window from its table.
 */
struct display_close
{
	uint32_t window_id;
};

/*
 * DISPLAY_KEY payload (display → client).
 */
struct display_key
{
	uint32_t window_id;
	uint32_t keycode;
	uint8_t pressed; /* 1 = down, 0 = up */
};

/*
 * DISPLAY_MOUSE payload (display → client).
 */
struct display_mouse_ev
{
	uint32_t window_id;
	int16_t x, y; /* position relative to window origin */
	int16_t dx, dy;
	uint8_t buttons;
};

/*
 * DISPLAY_QUERY payload (client → display).
 * reply: client's MPI mailbox for DISPLAY_INFO response.
 */
struct display_query
{
	char reply[64];
};

/*
 * DISPLAY_INFO payload (display → client).
 */
struct display_info
{
	uint32_t screen_w;
	uint32_t screen_h;
	uint8_t bpp;
};

/*
 * DISPLAY_RAISE payload (client → display).
 * Ask the compositor to raise and focus the given window.
 */
struct display_raise
{
	uint32_t window_id;
};

/*
 * DISPLAY_SETTITLE payload (client → display).
 * Replace the title-bar text of an existing window (e.g. a terminal marking
 * its shell as exited).  Ignored for no_decor windows.
 */
struct display_settitle
{
	uint32_t window_id;
	char title[64];
};

/*
 * DISPLAY_SET_USER payload (client → display).
 * vlogin sets the active session user so the compositor renders that user's
 * desktop (per-user settings); an empty name resets to the system default.
 */
struct display_set_user
{
	char user[64];
};

/*
 * DISPLAY_NOTIFY payload (display → taskbar).
 * Sent whenever a decorated window is added or removed.
 * added=1: window opened; added=0: window closed.
 */
struct display_notify
{
	uint32_t window_id;
	uint8_t added;
	char title[64];
};

#endif /* _DISPLAY_PROTO_H */
