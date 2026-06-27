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
 * bin/cubitaire — "Cubitaire", Patience for Views.
 *
 * A complete Klondike solitaire (draw-1, drag-and-drop) written as a native
 * views client.  The full 52-card deck is drawn PROCEDURALLY with objGFX — no
 * bitmap assets: rounded-rect bodies, scalable-font ranks, and suit pips built
 * from circles/triangles/diamonds.  This keeps the disk image asset-free and the
 * cards crisp at any size.
 *
 * Controls:
 *   - Click the stock (top-left) to turn a card to the waste; click the empty
 *     stock to recycle the waste.
 *   - Drag a face-up card (or a valid descending alt-colour run) between piles.
 *   - Double-click a card to send it straight to its foundation.
 *   - N = new game, M = mute/unmute music, 1-3 = pick a music track, ESC = quit.
 *
 * Audio (square-wave synth over libaudio / /dev/audio, like bin/tessera): action
 * cues (deal, draw, place, foundation chime, win fanfare) plus a toggleable calm
 * background track.  Silent if no audio device is present.
 */

extern "C"
{
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/mpi.h>
#include <sys/sched.h>
#include <views/display_proto.h>
#include <audio/audio.h>
}
#include <objgfx/objgfx.h>
#include <objgfx/ogScalableFont.h>

#define RGB(r, g, b) ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))

/* ── Card / table geometry (pixels) ─────────────────────────────────────────── */
#define CARD_W 72
#define CARD_H 98
#define MARGIN 18
#define GAP 14
#define TOP_Y 18
#define TAB_Y (TOP_Y + CARD_H + 22) /* first tableau row */
#define FAN_UP 24                   /* vertical step between face-up cards */
#define FAN_DOWN 8                  /* vertical step between face-down cards */
#define WIN_W (MARGIN * 2 + 7 * CARD_W + 6 * GAP)
#define WIN_H 700

/* Column x of the 7-wide grid: stock=0, waste=1, [2 gap], foundations=3..6;
 * tableau piles reuse the same 0..6 columns on the lower row. */
#define COL_X(c) (MARGIN + (c) * (CARD_W + GAP))
#define FOUND_COL(f) (3 + (f))

/* Palette. */
#define C_FELT RGB(28, 110, 58)
#define C_FELT_HI RGB(36, 124, 68)
#define C_CARD RGB(248, 248, 244)
#define C_BORDER RGB(36, 36, 40)
#define C_RED RGB(196, 30, 36)
#define C_BLACK RGB(24, 24, 28)
#define C_BACK RGB(34, 64, 150)
#define C_BACK_HI RGB(86, 120, 210)
#define C_SLOT RGB(22, 92, 50)
#define C_WHITE RGB(240, 240, 240)
#define C_GOLD RGB(232, 200, 96)

enum
{
	SPADE = 0,
	HEART = 1,
	DIAMOND = 2,
	CLUB = 3
};

#define FONT_RANK "/var/fonts/DejaVuSans-Bold.ttf"
#define FONT_UI "/var/fonts/DejaVuSans.ttf"
#define DBL_MS 380         /* double-click window (ms) */
#define MUSIC_POLL_TICKS 5 /* idle wake cadence while music plays (~50 ms @ 100 Hz) */

/* ── Model ──────────────────────────────────────────────────────────────────── */
struct Card
{
	uint8_t rank; /* 0=A, 1..9 = 2..10, 10=J, 11=Q, 12=K */
	uint8_t suit; /* SPADE/HEART/DIAMOND/CLUB */
	uint8_t up;   /* 1 = face up */
};

/* A pile is a fixed stack (at most the whole deck) plus a count. */
struct Pile
{
	Card c[52];
	int n;
};

static Pile g_stock, g_waste;
static Pile g_found[4]; /* foundations (one suit each, built A..K) */
static Pile g_tab[7];   /* tableau columns */

static ogSurface g_surf;      /* drawing target: a private back buffer (see claim_window) */
static void *g_shm_base;      /* the compositor's shared-memory front buffer */
static uint32_t *g_backbuf;   /* raw back-buffer pixels g_surf is attached to */
static uint32_t g_fb_bytes;   /* framebuffer size in bytes (w*h*4) */
static bool g_dbuf = false;   /* true when g_surf is a separate back buffer (double-buffered) */
static ogScalableFont g_rank; /* corner + centre rank glyphs */
static ogScalableFont g_ui;   /* title / status text */

static char g_mbox[32] = "cubitaire";
static const char g_views[] = "views";
static uint32_t g_win_id;
static int g_won;
static unsigned g_seed;

/* Drag state: a lifted run of cards, removed from `from` until dropped. */
struct Drag
{
	int active;
	Pile *from;
	Card card[52];
	int n;
	int grab_dx, grab_dy; /* cursor offset within the top card */
	int x, y;             /* current cursor position */
	int start_x, start_y; /* cursor position at pickup (click vs drag) */
};
static Drag g_drag;

/* Double-click tracking. */
static long g_last_ms;
static Pile *g_last_pile;
static int g_last_idx;

/**
 * Monotonic millisecond clock for double-click timing.
 *
 * @return milliseconds since an arbitrary epoch, or 0 if the clock is unavailable.
 */
static long now_ms(void)
{
	struct timeval tv;
	if (gettimeofday(&tv, 0) != 0)
		return 0;
	return (long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/** @return non-zero if the suit is red (hearts or diamonds). */
static int is_red(uint8_t suit)
{
	return suit == HEART || suit == DIAMOND;
}

/* ── Audio: square-wave synth + calm background tracks ──────────────────────── */
#define AUDIO_SR 48000

static int g_audio_fd = -1;
static int g_music_on = 1;    /* background music enabled */
static int g_music_track = 0; /* index into g_tracks */
static int g_music_note;      /* current note within the track */
static long g_music_note_end; /* ms timestamp the current note ends */

struct music_note_t
{
	int freq; /* Hz; 0 = rest */
	int ms;
};

/* Track 1 — Für Elise (Beethoven), the calm opening phrase. */
static const music_note_t g_track1[] = {
    {659, 160}, {622, 160}, {659, 160}, {622, 160}, {659, 160}, {494, 160}, {587, 160},
    {523, 160}, {440, 320}, {0, 120},   {262, 160}, {330, 160}, {440, 160}, {494, 320},
    {0, 120},   {330, 160}, {415, 160}, {494, 160}, {523, 360}, {0, 200},
};
/* Track 2 — Minuet in G (Bach/Petzold), an unhurried dance. */
static const music_note_t g_track2[] = {
    {392, 360}, {294, 180}, {330, 180}, {370, 180}, {392, 180}, {440, 180}, {494, 360}, {392, 180},
    {440, 360}, {0, 100},   {494, 180}, {523, 180}, {587, 360}, {392, 180}, {392, 180}, {440, 180},
    {494, 180}, {523, 180}, {587, 180}, {494, 360}, {440, 180}, {392, 360}, {0, 200},
};
/* Track 3 — Ode to Joy (Beethoven), gentle and bright. */
static const music_note_t g_track3[] = {
    {330, 200},
    {330, 200},
    {349, 200},
    {392, 200},
    {392, 200},
    {349, 200},
    {330, 200},
    {294, 200},
    {262, 200},
    {262, 200},
    {294, 200},
    {330, 200},
    {330, 300},
    {294, 100},
    {294, 400},
    {0, 160},
};

struct track_t
{
	const music_note_t *notes;
	int len;
	const char *name;
};
static const track_t g_tracks[] = {
    {g_track1, (int)(sizeof(g_track1) / sizeof(g_track1[0])), "Fur Elise"},
    {g_track2, (int)(sizeof(g_track2) / sizeof(g_track2[0])), "Minuet in G"},
    {g_track3, (int)(sizeof(g_track3) / sizeof(g_track3[0])), "Ode to Joy"},
};
#define TRACK_COUNT ((int)(sizeof(g_tracks) / sizeof(g_tracks[0])))

/**
 * Render a square-wave tone into `buf` (signed 16-bit stereo @ AUDIO_SR).  A
 * 10 ms linear fade-out suppresses the click at note boundaries.
 *
 * @param max_pairs capacity of buf in stereo sample pairs.
 * @return number of stereo pairs written.
 */
static int gen_tone(int16_t *buf, int max_pairs, int freq, int ms)
{
	int pairs = (AUDIO_SR * ms) / 1000;
	if (pairs > max_pairs)
		pairs = max_pairs;
	if (freq <= 0)
	{
		memset(buf, 0, (size_t)(pairs * 4));
		return pairs;
	}
	int period = AUDIO_SR / freq;
	if (period < 2)
		period = 2;
	int half = period / 2;
	int fade = (AUDIO_SR * 10) / 1000;
	for (int i = 0; i < pairs; i++)
	{
		int16_t s = ((i % period) < half) ? 5200 : -5200;
		if (i >= pairs - fade && fade > 0)
			s = (int16_t)((int)s * (pairs - i) / fade);
		buf[i * 2] = s;
		buf[i * 2 + 1] = s;
	}
	return pairs;
}

/** Play a short SFX tone (buffered into the aural ring; returns promptly). */
static void sfx_play(int freq, int ms)
{
	if (g_audio_fd < 0 || ms <= 0)
		return;
	if (ms > 200)
		ms = 200;
	static int16_t buf[(AUDIO_SR / 5 + 1) * 2]; /* up to 200 ms stereo */
	int pairs = gen_tone(buf, AUDIO_SR / 5, freq, ms);
	audio_write(g_audio_fd, buf, pairs * 4);
}

/* Distinct cues for each game action. */
static void sfx_deal(void)
{
	for (int i = 0; i < 6; i++)
		sfx_play(620 - i * 40, 16);
}
static void sfx_draw(void)
{
	sfx_play(520, 18);
}
static void sfx_recycle(void)
{
	sfx_play(300, 30);
	sfx_play(420, 30);
}
static void sfx_place(void)
{
	sfx_play(300, 22);
}
static void sfx_invalid(void)
{
	sfx_play(150, 50);
}
static void sfx_foundation(void)
{
	sfx_play(700, 26);
	sfx_play(880, 40);
}
static void sfx_win(void)
{
	static const int notes[] = {523, 659, 784, 880, 1047};
	for (int i = 0; i < 5; i++)
		sfx_play(notes[i], 110);
}

/**
 * Advance background music by one note when the current note has elapsed.  Called
 * each loop iteration; a no-op while muted or between note boundaries, so it never
 * blocks (single notes buffer into the aural ring).
 */
static void music_tick(long t)
{
	if (g_audio_fd < 0 || !g_music_on)
		return;
	if (t < g_music_note_end)
		return;
	const track_t &tr = g_tracks[g_music_track];
	const music_note_t &nt = tr.notes[g_music_note];
	int ms = nt.ms > 250 ? 250 : nt.ms;
	static int16_t buf[(AUDIO_SR / 4 + 1) * 2]; /* up to 250 ms stereo */
	int pairs = gen_tone(buf, AUDIO_SR / 4, nt.freq, ms);
	audio_write(g_audio_fd, buf, pairs * 4);
	g_music_note_end = t + nt.ms;
	g_music_note = (g_music_note + 1) % tr.len;
}

/* ── Deal / shuffle ─────────────────────────────────────────────────────────── */

/**
 * Start a fresh game: build an ordered 52-card deck, Fisher-Yates shuffle it,
 * and deal the Klondike layout (column i gets i+1 cards, only the top face up).
 */
static void new_game(void)
{
	Card deck[52];
	int i, k, t;

	for (i = 0; i < 52; i++)
	{
		deck[i].rank = (uint8_t)(i % 13);
		deck[i].suit = (uint8_t)(i / 13);
		deck[i].up = 0;
	}
	/* Fisher-Yates; reseed each deal so successive games differ. */
	g_seed = g_seed * 1103515245u + 12345u + (unsigned)now_ms();
	srand(g_seed ^ (unsigned)getpid());
	for (i = 51; i > 0; i--)
	{
		k = rand() % (i + 1);
		Card tmp = deck[i];
		deck[i] = deck[k];
		deck[k] = tmp;
	}

	g_stock.n = g_waste.n = 0;
	for (i = 0; i < 4; i++)
		g_found[i].n = 0;
	for (i = 0; i < 7; i++)
		g_tab[i].n = 0;

	k = 0;
	for (t = 0; t < 7; t++)
	{
		for (i = 0; i <= t; i++)
		{
			Card c = deck[k++];
			c.up = (i == t); /* top card of each column face up */
			g_tab[t].c[g_tab[t].n++] = c;
		}
	}
	/* Remaining 24 cards form the stock, all face down. */
	while (k < 52)
	{
		Card c = deck[k++];
		c.up = 0;
		g_stock.c[g_stock.n++] = c;
	}
	g_won = 0;
	g_drag.active = 0;
	g_last_pile = 0;
	sfx_deal();
}

/* ── Procedural card art ────────────────────────────────────────────────────── */

/**
 * Draw one suit pip centred at (cx, cy).  `h` is the pip half-extent; the four
 * suits are composed from filled circles, triangles and a diamond so the deck
 * needs no bitmap assets.
 */
static void draw_pip(uint8_t suit, int cx, int cy, int h, uint32_t col)
{
	switch (suit)
	{
		case HEART:
			g_surf.ogFillCircle(cx - h / 2, cy - h / 3, (uint32_t)(h / 2 + 1), col);
			g_surf.ogFillCircle(cx + h / 2, cy - h / 3, (uint32_t)(h / 2 + 1), col);
			g_surf.ogFillTriangle(cx - h, cy - h / 4, cx + h, cy - h / 4, cx, cy + h, col);
			break;
		case DIAMOND:
			g_surf.ogFillTriangle(cx, cy - h, cx + (h * 3) / 4, cy, cx - (h * 3) / 4, cy, col);
			g_surf.ogFillTriangle(cx - (h * 3) / 4, cy, cx + (h * 3) / 4, cy, cx, cy + h, col);
			break;
		case CLUB:
			g_surf.ogFillCircle(cx, cy - h / 2, (uint32_t)(h / 2 + 1), col);
			g_surf.ogFillCircle(cx - (h * 3) / 5, cy + h / 5, (uint32_t)(h / 2 + 1), col);
			g_surf.ogFillCircle(cx + (h * 3) / 5, cy + h / 5, (uint32_t)(h / 2 + 1), col);
			g_surf.ogFillTriangle(cx - h / 4, cy + h / 6, cx + h / 4, cy + h / 6, cx, cy + h, col);
			g_surf.ogFillRect(cx - h / 6, cy, cx + h / 6, cy + h, col);
			break;
		default: /* SPADE — heart pointing up, plus a stem */
			g_surf.ogFillTriangle(cx, cy - h, cx - h, cy + h / 4, cx + h, cy + h / 4, col);
			g_surf.ogFillCircle(cx - h / 2, cy + h / 5, (uint32_t)(h / 2 + 1), col);
			g_surf.ogFillCircle(cx + h / 2, cy + h / 5, (uint32_t)(h / 2 + 1), col);
			g_surf.ogFillTriangle(cx - h / 4, cy + h / 6, cx + h / 4, cy + h / 6, cx, cy + h, col);
			g_surf.ogFillRect(cx - h / 6, cy + h / 3, cx + h / 6, cy + h, col);
			break;
	}
}

/** @return the rank label ("A", "2".."10", "J", "Q", "K"). */
static const char *rank_str(uint8_t rank)
{
	static const char *names[13] = {"A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
	return names[rank < 13 ? rank : 0];
}

/** Draw a single face-up card with its body, border, corner rank and centre pip. */
static void draw_card_face(const Card &c, int x, int y)
{
	uint32_t ink = is_red(c.suit) ? C_RED : C_BLACK;

	g_surf.ogFillRoundRect(x, y, x + CARD_W, y + CARD_H, 8, C_CARD);
	g_surf.ogRect(x, y, x + CARD_W, y + CARD_H, C_BORDER);

	const char *rs = rank_str(c.rank);
	if (g_rank.IsValid())
	{
		g_rank.SetFGColor((ink >> 16) & 0xff, (ink >> 8) & 0xff, ink & 0xff);
		g_rank.PutString(g_surf, x + 6, y + 4, rs);
	}
	/* Small corner pip under the rank. */
	draw_pip(c.suit, x + 13, y + 34, 6, ink);
	/* Big centre pip — the card's identity at a glance. */
	draw_pip(c.suit, x + CARD_W / 2, y + CARD_H / 2 + 6, CARD_W / 4, ink);
}

/** Draw a face-down card: a coloured back with a simple woven pattern. */
static void draw_card_back(int x, int y)
{
	g_surf.ogFillRoundRect(x, y, x + CARD_W, y + CARD_H, 8, C_BACK);
	g_surf.ogRect(x, y, x + CARD_W, y + CARD_H, C_BORDER);
	for (int yy = y + 6; yy < y + CARD_H - 6; yy += 8)
		g_surf.ogHLine(x + 6, x + CARD_W - 6, yy, C_BACK_HI);
	for (int xx = x + 6; xx < x + CARD_W - 6; xx += 8)
		g_surf.ogVLine(xx, y + 6, y + CARD_H - 6, C_BACK_HI);
	g_surf.ogRect(x + 5, y + 5, x + CARD_W - 5, y + CARD_H - 5, C_WHITE);
}

/** Draw an empty pile placeholder (rounded outline on the felt). */
static void draw_slot(int x, int y)
{
	g_surf.ogFillRoundRect(x, y, x + CARD_W, y + CARD_H, 8, C_SLOT);
	g_surf.ogRect(x, y, x + CARD_W, y + CARD_H, C_FELT_HI);
}

/** Draw either a card face or back depending on its state. */
static void draw_card(const Card &c, int x, int y)
{
	if (c.up)
		draw_card_face(c, x, y);
	else
		draw_card_back(x, y);
}

/* ── Layout helpers ─────────────────────────────────────────────────────────── */

/** @return the y of tableau card `i` in column `t`, accounting for fan spacing. */
static int tab_card_y(int t, int i)
{
	int y = TAB_Y;
	for (int k = 0; k < i && k < g_tab[t].n; k++)
		y += g_tab[t].c[k].up ? FAN_UP : FAN_DOWN;
	return y;
}

/* ── Render ─────────────────────────────────────────────────────────────────── */

/** Tell the compositor to recomposite the whole window. */
static void flip(void)
{
	mpi_message_t msg;
	memset(&msg, 0, sizeof(msg));
	msg.header = DISPLAY_FLIP;
	struct display_flip *fl = (struct display_flip *)msg.data;
	fl->window_id = g_win_id;
	fl->dirty_x = 0;
	fl->dirty_y = 0;
	fl->dirty_w = WIN_W;
	fl->dirty_h = WIN_H;
	mpi_postMessage((char *)g_views, DISPLAY_FLIP, &msg);
}

/** Paint the whole table: felt, all piles, any drag stack, and the status bar. */
static void render(void)
{
	int i, t;

	g_surf.ogFillRect(0, 0, WIN_W - 1, WIN_H - 1, C_FELT);

	/* Stock. */
	if (g_stock.n > 0)
		draw_card_back(COL_X(0), TOP_Y);
	else
		draw_slot(COL_X(0), TOP_Y);

	/* Waste (draw-1: only the top card shows). */
	if (g_waste.n > 0)
		draw_card(g_waste.c[g_waste.n - 1], COL_X(1), TOP_Y);
	else
		draw_slot(COL_X(1), TOP_Y);

	/* Foundations. */
	for (i = 0; i < 4; i++)
	{
		int fx = COL_X(FOUND_COL(i));
		if (g_found[i].n > 0)
			draw_card(g_found[i].c[g_found[i].n - 1], fx, TOP_Y);
		else
		{
			draw_slot(fx, TOP_Y);
			/* Ghost pip hinting where each suit lands. */
			draw_pip((uint8_t)i, fx + CARD_W / 2, TOP_Y + CARD_H / 2, CARD_W / 5, C_FELT_HI);
		}
	}

	/* Tableau columns. */
	for (t = 0; t < 7; t++)
	{
		int tx = COL_X(t);
		if (g_tab[t].n == 0)
			draw_slot(tx, TAB_Y);
		for (i = 0; i < g_tab[t].n; i++)
			draw_card(g_tab[t].c[i], tx, tab_card_y(t, i));
	}

	/* Drag stack rides above everything else. */
	if (g_drag.active)
	{
		int dx = g_drag.x - g_drag.grab_dx;
		int dy = g_drag.y - g_drag.grab_dy;
		for (i = 0; i < g_drag.n; i++)
			draw_card(g_drag.card[i], dx, dy + i * FAN_UP);
	}

	/* Status bar. */
	if (g_ui.IsValid())
	{
		int by = WIN_H - 26;
		g_surf.ogFillRect(0, by - 4, WIN_W - 1, WIN_H - 1, C_FELT_HI);
		g_ui.SetFGColor(245, 245, 240);
		if (g_won)
		{
			g_ui.SetFGColor((C_GOLD >> 16) & 0xff, (C_GOLD >> 8) & 0xff, C_GOLD & 0xff);
			g_ui.PutString(g_surf, MARGIN, by, "You win!  Press N for a new deal.");
		}
		else
		{
			g_ui.PutString(g_surf,
			               MARGIN,
			               by,
			               "drag \xE2\x80\xA2 dbl-click \xE2\x86\x92 foundation \xE2\x80\xA2 N new "
			               "\xE2\x80\xA2 M music \xE2\x80\xA2 1-3 track \xE2\x80\xA2 ESC quit");
		}

		/* Right-aligned music indicator (♪). */
		char mus[48];
		if (g_music_on && g_audio_fd >= 0)
			snprintf(mus, sizeof(mus), "\xE2\x99\xAA %s", g_tracks[g_music_track].name);
		else
			snprintf(mus, sizeof(mus), "\xE2\x99\xAA off");
		uint32_t mw = g_ui.TextWidth(mus);
		g_ui.SetFGColor(210, 226, 210);
		g_ui.PutString(g_surf, WIN_W - MARGIN - (int)mw, by, mus);
	}

	/* Publish the finished frame: one copy of the back buffer into the shared
	 * front buffer, so the compositor never reads a partially-drawn window. */
	if (g_dbuf)
		memcpy(g_shm_base, g_backbuf, g_fb_bytes);
	flip();
}

/* ── Rules ──────────────────────────────────────────────────────────────────── */

/** @return non-zero if `c` may be placed on foundation pile `f`. */
static int can_to_foundation(const Pile *f, const Card &c)
{
	if (f->n == 0)
		return c.rank == 0; /* only an Ace starts a foundation */
	const Card &t = f->c[f->n - 1];
	return c.suit == t.suit && c.rank == t.rank + 1;
}

/** @return non-zero if `c` may be placed on tableau column `p`. */
static int can_to_tableau(const Pile *p, const Card &c)
{
	if (p->n == 0)
		return c.rank == 12; /* only a King fills an empty column */
	const Card &t = p->c[p->n - 1];
	return t.up && is_red(c.suit) != is_red(t.suit) && c.rank + 1 == t.rank;
}

/** Flip the now-top card of a tableau column face up if it is hidden. */
static void expose_top(Pile *p)
{
	if (p->n > 0 && !p->c[p->n - 1].up)
		p->c[p->n - 1].up = 1;
}

/** @return non-zero once every foundation holds a full A..K suit. */
static int check_win(void)
{
	int total = 0;
	for (int i = 0; i < 4; i++)
		total += g_found[i].n;
	return total == 52;
}

/* ── Stock / waste ──────────────────────────────────────────────────────────── */

/** Turn one card from stock to waste, or recycle the waste when stock is empty. */
static void click_stock(void)
{
	if (g_stock.n > 0)
	{
		Card c = g_stock.c[--g_stock.n];
		c.up = 1;
		g_waste.c[g_waste.n++] = c;
		sfx_draw();
	}
	else
	{
		while (g_waste.n > 0)
		{
			Card c = g_waste.c[--g_waste.n];
			c.up = 0;
			g_stock.c[g_stock.n++] = c;
		}
		sfx_recycle();
	}
}

/* ── Hit testing ────────────────────────────────────────────────────────────── */

/** @return non-zero if (px,py) lies within the card rect at (x,y). */
static int in_card(int px, int py, int x, int y)
{
	return px >= x && px < x + CARD_W && py >= y && py < y + CARD_H;
}

/**
 * Send a single card straight to a valid foundation (double-click / auto-play).
 *
 * @return non-zero if the card was placed.
 */
static int auto_to_foundation(Pile *from)
{
	if (from->n == 0)
		return 0;
	const Card &c = from->c[from->n - 1];
	if (!c.up)
		return 0;
	for (int f = 0; f < 4; f++)
	{
		if (can_to_foundation(&g_found[f], c))
		{
			g_found[f].c[g_found[f].n++] = c;
			from->n--;
			expose_top(from);
			sfx_foundation();
			if (check_win())
			{
				g_won = 1;
				sfx_win();
			}
			return 1;
		}
	}
	return 0;
}

/** Begin dragging cards [idx..n) off `from`, recording the grab offset. */
static void begin_drag(Pile *from, int idx, int px, int py, int card_x, int card_y)
{
	g_drag.active = 1;
	g_drag.from = from;
	g_drag.n = 0;
	for (int i = idx; i < from->n; i++)
		g_drag.card[g_drag.n++] = from->c[i];
	from->n = idx;
	g_drag.grab_dx = px - card_x;
	g_drag.grab_dy = py - card_y;
	g_drag.x = g_drag.start_x = px;
	g_drag.y = g_drag.start_y = py;
}

/** Return the lifted drag stack to its origin pile (invalid drop / cancel). */
static void cancel_drag(void)
{
	for (int i = 0; i < g_drag.n; i++)
		g_drag.from->c[g_drag.from->n++] = g_drag.card[i];
	g_drag.active = 0;
}

/**
 * Resolve a button-press: stock click, double-click auto-play, or pick up a card.
 *
 * @return non-zero if the table changed and needs repainting.
 */
static int on_press(int px, int py)
{
	long t = now_ms();
	Pile *hit_pile = 0;
	int hit_idx = -1;
	int cx = 0, cy = 0;

	/* Stock. */
	if (in_card(px, py, COL_X(0), TOP_Y))
	{
		click_stock();
		g_last_pile = 0;
		return 1;
	}

	/* Waste top. */
	if (g_waste.n > 0 && in_card(px, py, COL_X(1), TOP_Y))
	{
		hit_pile = &g_waste;
		hit_idx = g_waste.n - 1;
		cx = COL_X(1);
		cy = TOP_Y;
	}

	/* Foundation tops (allow pulling a card back down). */
	for (int f = 0; hit_idx < 0 && f < 4; f++)
	{
		int fx = COL_X(FOUND_COL(f));
		if (g_found[f].n > 0 && in_card(px, py, fx, TOP_Y))
		{
			hit_pile = &g_found[f];
			hit_idx = g_found[f].n - 1;
			cx = fx;
			cy = TOP_Y;
		}
	}

	/* Tableau — topmost covering card wins. */
	for (int col = 0; hit_idx < 0 && col < 7; col++)
	{
		int tx = COL_X(col);
		if (px < tx || px >= tx + CARD_W)
			continue;
		for (int i = g_tab[col].n - 1; i >= 0; i--)
		{
			int y = tab_card_y(col, i);
			int bottom = (i == g_tab[col].n - 1) ? y + CARD_H : tab_card_y(col, i + 1);
			if (py >= y && py < bottom && g_tab[col].c[i].up)
			{
				hit_pile = &g_tab[col];
				hit_idx = i;
				cx = tx;
				cy = y;
				break;
			}
		}
	}

	if (!hit_pile)
	{
		g_last_pile = 0;
		return 0;
	}

	/* Double-click on a top card → foundation. */
	int is_top = (hit_idx == hit_pile->n - 1);
	if (is_top && g_last_pile == hit_pile && g_last_idx == hit_idx && (t - g_last_ms) <= DBL_MS)
	{
		g_last_pile = 0;
		if (auto_to_foundation(hit_pile))
			return 1;
	}
	g_last_ms = t;
	g_last_pile = hit_pile;
	g_last_idx = hit_idx;

	begin_drag(hit_pile, hit_idx, px, py, cx, cy);
	return 1;
}

/**
 * Resolve a button-release: try to drop the drag stack, else snap it back.
 *
 * @return non-zero if the table needs repainting.
 */
static int on_release(void)
{
	if (!g_drag.active)
		return 0;

	int dx = g_drag.x - g_drag.grab_dx;
	int cx = dx + CARD_W / 2; /* centre of the lifted top card */
	int cy = (g_drag.y - g_drag.grab_dy) + CARD_H / 2;
	const Card &bottom = g_drag.card[0];

	/* Foundations accept a single card. */
	if (g_drag.n == 1)
	{
		for (int f = 0; f < 4; f++)
		{
			int fx = COL_X(FOUND_COL(f));
			if (cx >= fx && cx < fx + CARD_W && cy >= TOP_Y && cy < TOP_Y + CARD_H &&
			    can_to_foundation(&g_found[f], bottom))
			{
				g_found[f].c[g_found[f].n++] = bottom;
				g_drag.active = 0;
				expose_top(g_drag.from);
				sfx_foundation();
				if (check_win())
				{
					g_won = 1;
					sfx_win();
				}
				return 1;
			}
		}
	}

	/* Tableau columns. */
	for (int col = 0; col < 7; col++)
	{
		int tx = COL_X(col);
		if (cx < tx || cx >= tx + CARD_W || cy < TAB_Y)
			continue;
		if (can_to_tableau(&g_tab[col], bottom))
		{
			for (int i = 0; i < g_drag.n; i++)
				g_tab[col].c[g_tab[col].n++] = g_drag.card[i];
			g_drag.active = 0;
			expose_top(g_drag.from);
			sfx_place();
			return 1;
		}
	}

	/* No valid target: snap back.  Only thunk if the user actually dragged —
	 * a plain click that didn't move shouldn't make a rejection sound. */
	int moved = (g_drag.x - g_drag.start_x) * (g_drag.x - g_drag.start_x) +
	            (g_drag.y - g_drag.start_y) * (g_drag.y - g_drag.start_y);
	cancel_drag();
	if (moved > 36)
		sfx_invalid();
	return 1;
}

/* ── views plumbing ─────────────────────────────────────────────────────────── */

/**
 * Claim a fixed-size window from the compositor and attach our surface.
 *
 * @return 0 on success, -1 if the compositor denies the request.
 */
static int claim_window(void)
{
	snprintf(g_mbox, sizeof(g_mbox), "cubitaire.%d", (int)getpid());
	if (mpi_createMbox((char *)g_mbox) != 0)
	{
		printf("cubitaire: mpi_createMbox failed\n");
		return -1;
	}

	mpi_message_t msg;
	memset(&msg, 0, sizeof(msg));
	msg.header = DISPLAY_CLAIM;
	struct display_claim_req *req = (struct display_claim_req *)msg.data;
	req->x = 0;
	req->y = 0;
	req->w = WIN_W;
	req->h = WIN_H;
	req->sender_pid = getpid();
	strncpy(req->title, "Cubitaire", sizeof(req->title) - 1);
	strncpy(req->reply, g_mbox, sizeof(req->reply) - 1);
	/* Fixed size (min == max), and we need drag tracking. */
	req->min_w = req->max_w = WIN_W;
	req->min_h = req->max_h = WIN_H;
	req->wants_motion = 1;

	while (mpi_postMessage((char *)g_views, DISPLAY_CLAIM, &msg) != 0)
		sched_yield();

	mpi_message_t reply;
	while (mpi_fetchMessage((char *)g_mbox, &reply) != 0)
		sched_yield();
	if (reply.header != DISPLAY_ACK)
	{
		printf("cubitaire: compositor denied window\n");
		return -1;
	}

	struct display_ack *ack = (struct display_ack *)reply.data;
	g_win_id = ack->window_id;

	/* Double-buffer: render into a private off-screen surface, then copy the
	 * finished frame into the shared buffer in one pass.  The compositor only
	 * ever sees complete frames (the shared buffer is untouched during the
	 * full clear+redraw), which removes the drag flicker.  Fall back to drawing
	 * straight into the shared buffer if the back-buffer allocation fails. */
	g_shm_base = ack->shm_base;
	g_fb_bytes = (uint32_t)ack->w * (uint32_t)ack->h * 4u;
	g_backbuf = (uint32_t *)malloc(g_fb_bytes);
	if (g_backbuf != NULL)
	{
		/* Draw into the private back buffer; render() memcpy's the finished frame
		 * into the shared buffer verbatim, so the compositor never sees a
		 * partially-drawn window (the drag-flicker fix). */
		g_surf.ogAttach(g_backbuf, (uint32_t)ack->w, (uint32_t)ack->h, OG_PIXFMT_32BPP);
		g_dbuf = true;
	}
	else
	{
		/* Out of memory: fall back to drawing straight into the shared buffer. */
		g_surf.ogAttach(ack->shm_base, (uint32_t)ack->w, (uint32_t)ack->h, OG_PIXFMT_32BPP);
		g_dbuf = false;
	}
	return 0;
}

/** Release the window and tear down our mailbox before exiting. */
static void release_window(void)
{
	mpi_message_t msg;
	memset(&msg, 0, sizeof(msg));
	msg.header = DISPLAY_RELEASE;
	struct display_release *rel = (struct display_release *)msg.data;
	rel->window_id = g_win_id;
	mpi_postMessage((char *)g_views, DISPLAY_RELEASE, &msg);
	mpi_destroyMbox((char *)g_mbox);
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	if (claim_window() != 0)
		return 1;

	if (!g_rank.Load(FONT_RANK, 20))
		g_rank.Load(FONT_UI, 20); /* fall back if the bold face is absent */
	g_ui.Load(FONT_UI, 16);

	g_audio_fd = audio_open("/dev/audio"); /* -1 if no audio device: game stays silent */
	g_music_note_end = now_ms();

	new_game();
	render();

	uint8_t prev_buttons = 0;
	for (;;)
	{
		/* Advance background music, then block for input.  While music plays we
		 * wake periodically (MUSIC_POLL_TICKS) to feed the next note; when muted
		 * we block indefinitely so an idle game costs no CPU. */
		music_tick(now_ms());
		uint32_t timeout = (g_music_on && g_audio_fd >= 0) ? MUSIC_POLL_TICKS : 0;

		mpi_message_t ev;
		if (mpi_waitMessage((char *)g_mbox, &ev, timeout) != 0)
			continue; /* timeout — loop to tick music */

		switch (ev.header)
		{
			case DISPLAY_CLOSE:
				release_window();
				return 0;

			case DISPLAY_KEY:
			{
				struct display_key *k = (struct display_key *)ev.data;
				if (!k->pressed)
					break;
				if (k->keycode == 27) /* ESC */
				{
					release_window();
					return 0;
				}
				if (k->keycode == 'n' || k->keycode == 'N')
				{
					new_game();
					render();
				}
				else if (k->keycode == 'm' || k->keycode == 'M')
				{
					g_music_on = !g_music_on;
					render();
				}
				else if (k->keycode >= '1' && k->keycode <= '3')
				{
					g_music_track = (int)(k->keycode - '1') % TRACK_COUNT;
					g_music_note = 0;
					g_music_note_end = now_ms();
					g_music_on = 1;
					render();
				}
				break;
			}

			case DISPLAY_MOUSE:
			{
				struct display_mouse_ev *m = (struct display_mouse_ev *)ev.data;
				uint8_t left = m->buttons & 1;
				int dirty = 0;

				if (left && !(prev_buttons & 1))
					dirty |= on_press(m->x, m->y);
				else if (!left && (prev_buttons & 1))
					dirty |= on_release();
				else if (left && g_drag.active)
				{
					g_drag.x = m->x;
					g_drag.y = m->y;
					dirty = 1;
				}
				prev_buttons = m->buttons;
				if (dirty)
					render();
				break;
			}

			default:
				break;
		}
	}
}
