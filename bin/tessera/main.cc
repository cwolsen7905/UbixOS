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
 * Tessera — Tetris clone for the UbixOS views compositor.
 * "Tessera" is derived from Greek/Latin for "four" + "tile".
 *
 * Controls: arrow keys to move/rotate, space = hard drop,
 *           P = pause, Q = quit.
 * Audio:    Korobeiniki (Tetris Theme A) + square-wave SFX.
 */

#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sys/time.h>
#include <ubix/mailbox.hh>
#include <ubix/sched.hh>
#include <ubix/process.hh>
#include <views/display.hh>
#include <objgfx/objgfx.h>
#include <objgfx/ogFont.h>
#include <objgfx/ogPixelFmt.h>
#include <audio/audio.h>

/* ------------------------------------------------------------------ */
/* Layout                                                               */
/* ------------------------------------------------------------------ */

#define CELL_SZ      24
#define BOARD_W      10
#define BOARD_H      20
#define BOARD_PX_W   (CELL_SZ * BOARD_W)       /* 240 */
#define BOARD_PX_H   (CELL_SZ * BOARD_H)       /* 480 */
#define SIDE_X       (BOARD_PX_W + 8)          /* 248 */
#define SIDE_W       112
#define WIN_W        (BOARD_PX_W + SIDE_W + 8) /* 360 */
#define WIN_H        BOARD_PX_H                /* 480 */

#define FONT_PATH    "/var/fonts/ROM8X8.DPF"
#define FONT_H       8
#define FONT_W       8

/* ------------------------------------------------------------------ */
/* Colors (0x00RRGGBB — top byte ignored by compositor)                 */
/* ------------------------------------------------------------------ */

#define COL_BG       0x00101020u
#define COL_GRID     0x001A1A30u
#define COL_BORDER   0x002244AAu
#define COL_SIDEBAR  0x00141428u
#define COL_GHOST    0x00252545u

static const uint32_t g_piece_colors[8] = {
	COL_BG,       /* 0 = empty      */
	0x0000CCCCu,  /* 1 = I  cyan    */
	0x00CCCC00u,  /* 2 = O  yellow  */
	0x00990099u,  /* 3 = T  purple  */
	0x0000AA33u,  /* 4 = S  green   */
	0x00BB1100u,  /* 5 = Z  red     */
	0x000033CCu,  /* 6 = J  blue    */
	0x00EE7700u,  /* 7 = L  orange  */
};

/* ------------------------------------------------------------------ */
/* Tetromino data                                                       */
/* g_pieces[type][rotation][cell] = {col_offset, row_offset}           */
/* All offsets within a 4×4 bounding box; origin = top-left corner.    */
/* ------------------------------------------------------------------ */

static const int g_pieces[7][4][4][2] = {
	/* 0 = I */
	{
		{{0,1},{1,1},{2,1},{3,1}},
		{{2,0},{2,1},{2,2},{2,3}},
		{{0,2},{1,2},{2,2},{3,2}},
		{{1,0},{1,1},{1,2},{1,3}},
	},
	/* 1 = O */
	{
		{{1,0},{2,0},{1,1},{2,1}},
		{{1,0},{2,0},{1,1},{2,1}},
		{{1,0},{2,0},{1,1},{2,1}},
		{{1,0},{2,0},{1,1},{2,1}},
	},
	/* 2 = T */
	{
		{{1,0},{0,1},{1,1},{2,1}},
		{{1,0},{1,1},{2,1},{1,2}},
		{{0,1},{1,1},{2,1},{1,2}},
		{{1,0},{0,1},{1,1},{1,2}},
	},
	/* 3 = S */
	{
		{{1,0},{2,0},{0,1},{1,1}},
		{{1,0},{1,1},{2,1},{2,2}},
		{{1,1},{2,1},{0,2},{1,2}},
		{{0,0},{0,1},{1,1},{1,2}},
	},
	/* 4 = Z */
	{
		{{0,0},{1,0},{1,1},{2,1}},
		{{2,0},{1,1},{2,1},{1,2}},
		{{0,1},{1,1},{1,2},{2,2}},
		{{1,0},{0,1},{1,1},{0,2}},
	},
	/* 5 = J */
	{
		{{0,0},{0,1},{1,1},{2,1}},
		{{1,0},{2,0},{1,1},{1,2}},
		{{0,1},{1,1},{2,1},{2,2}},
		{{1,0},{1,1},{0,2},{1,2}},
	},
	/* 6 = L */
	{
		{{2,0},{0,1},{1,1},{2,1}},
		{{1,0},{1,1},{1,2},{2,2}},
		{{0,1},{1,1},{2,1},{0,2}},
		{{0,0},{1,0},{1,1},{1,2}},
	},
};

/* ------------------------------------------------------------------ */
/* Audio — square-wave synthesis + Korobeiniki melody                   */
/* ------------------------------------------------------------------ */

#define AUDIO_SR     48000

struct music_note_t {
	int freq; /* Hz; 0 = rest */
	int ms;   /* duration in milliseconds */
};

/* Track 1 — Korobeiniki (Tetris Theme A) */
static const music_note_t g_melody[] = {
	{659,200},{494,100},{523,100},{587,200},{523,100},{494,100},
	{440,200},{440,100},{523,100},{659,200},{587,100},{523,100},
	{494,300},{523,100},{587,200},{659,200},{523,200},{440,200},{440,200},
	{0,100},
	{587,200},{698,100},{880,200},{784,100},{698,100},
	{659,300},{523,100},{659,200},{587,100},{523,100},
	{494,200},{494,100},{523,100},{587,200},{659,200},
	{523,200},{440,200},{440,200},{0,200},
};
static const int g_melody_len = (int)(sizeof(g_melody) / sizeof(g_melody[0]));

/* Track 2 — Minuet in G (Bach/Petzold) */
static const music_note_t g_melody_2[] = {
	{392,400},{294,200},{330,200},{370,200},{392,200},{440,200},
	{494,400},{392,200},{440,400},{0,100},{494,200},{523,200},
	{587,400},{392,200},{392,200},{440,200},{494,200},{523,200},{587,200},
	{494,400},{440,200},{392,400},{294,200},{494,200},{587,200},
	{392,400},{0,100},
	{330,400},{262,200},{294,200},{330,200},{370,200},
	{392,400},{330,200},{370,400},{294,200},{440,200},
	{294,400},{220,200},{247,200},{262,200},{294,200},{330,200},
	{370,400},{330,200},{294,400},{0,200},
};
static const int g_melody_2_len = (int)(sizeof(g_melody_2) / sizeof(g_melody_2[0]));

/* Track 3 — Ode to Joy (Beethoven 9th, 4th movement theme) */
static const music_note_t g_melody_3[] = {
	{330,200},{330,200},{349,200},{392,200},
	{392,200},{349,200},{330,200},{294,200},
	{262,200},{262,200},{294,200},{330,200},
	{330,300},{294,100},{294,400},
	{330,200},{330,200},{349,200},{392,200},
	{392,200},{349,200},{330,200},{294,200},
	{262,200},{262,200},{294,200},{330,200},
	{294,300},{262,100},{262,400},
	{294,200},{294,200},{330,200},{262,200},
	{294,200},{330,100},{349,100},{330,200},{262,200},
	{294,200},{330,100},{349,100},{330,200},{294,200},
	{262,200},{294,200},{196,400},{0,200},
};
static const int g_melody_3_len = (int)(sizeof(g_melody_3) / sizeof(g_melody_3[0]));

struct track_t {
	const music_note_t *notes;
	int                 len;
	const char         *name;
};

static const track_t g_tracks[] = {
	{ g_melody,   g_melody_len,   "Korobeiniki" },
	{ g_melody_2, g_melody_2_len, "Minuet in G" },
	{ g_melody_3, g_melody_3_len, "Ode to Joy"  },
};
static const int g_track_count = (int)(sizeof(g_tracks) / sizeof(g_tracks[0]));

/**
 * Generate a square-wave tone into buf (signed 16-bit stereo, AUDIO_SR Hz).
 * A 12 ms linear fade-out removes click artifacts at note boundaries.
 * @param max_pairs maximum stereo sample pairs buf can hold.
 * @return number of stereo sample pairs written.
 */
static int
gen_tone(int16_t *buf, int max_pairs, int freq, int ms)
{
	int pairs = (AUDIO_SR * ms) / 1000;
	if (pairs > max_pairs)
		pairs = max_pairs;
	if (freq <= 0) {
		memset(buf, 0, (size_t)(pairs * 4));
		return pairs;
	}
	int period = AUDIO_SR / freq;
	if (period < 2)
		period = 2;
	int half = period / 2;
	int fade = (AUDIO_SR * 12) / 1000;
	for (int i = 0; i < pairs; i++) {
		int16_t s = ((i % period) < half) ? 6000 : -6000;
		if (i >= pairs - fade && fade > 0)
			s = (int16_t)((int)s * (pairs - i) / fade);
		buf[i * 2]     = s;
		buf[i * 2 + 1] = s;
	}
	return pairs;
}

/**
 * Play a short SFX tone (≤ 200 ms) synchronously via the audio device.
 */
static void
sfx_play(int fd, int freq, int ms)
{
	if (fd < 0 || ms <= 0)
		return;
	if (ms > 200)
		ms = 200;
	static int16_t g_sfx_buf[(AUDIO_SR / 5 + 1) * 2]; /* 200 ms stereo */
	int pairs = gen_tone(g_sfx_buf, AUDIO_SR / 5, freq, ms);
	audio_write(fd, g_sfx_buf, pairs * 4);
}

/* ------------------------------------------------------------------ */
/* Game state                                                           */
/* ------------------------------------------------------------------ */

struct game_state_t {
	int  board[BOARD_H][BOARD_W]; /* 0=empty, 1-7=locked piece type+1 */
	int  cur_type;                /* 0-6  active piece type */
	int  cur_rot;                 /* 0-3  active rotation   */
	int  cur_x;                   /* board column of bbox left */
	int  cur_y;                   /* board row of bbox top     */
	int  next_type;               /* 0-6  next piece type   */
	int  score;
	int  level;
	int  total_lines;
	bool game_over;
	bool paused;
	/* timing */
	long drop_interval_ms;
	long last_drop_ms;
	/* music */
	int  music_track;
	int  music_note;
	long music_note_end_ms;
	/* audio */
	int  audio_fd;
	/* RNG: Knuth multiplicative LCG */
	unsigned rng_state;
};

static long
now_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, nullptr);
	return (long)(tv.tv_sec * 1000L + tv.tv_usec / 1000L);
}

static unsigned
rng_next(game_state_t &g)
{
	g.rng_state = g.rng_state * 1664525u + 1013904223u;
	return (g.rng_state >> 16) & 0x7FFFu;
}

static int
rand_piece(game_state_t &g)
{
	return (int)(rng_next(g) % 7u);
}

static long
drop_interval_for_level(int level)
{
	long ms = 800L - (long)(level - 1) * 70L;
	return (ms < 100L) ? 100L : ms;
}

/**
 * Check whether piece (type, rot) placed at board position (px, py) is valid.
 * @return true if all 4 cells are in-bounds and unoccupied.
 */
static bool
piece_fits(const game_state_t &g, int type, int rot, int px, int py)
{
	for (int i = 0; i < 4; i++) {
		int cx = px + g_pieces[type][rot][i][0];
		int cy = py + g_pieces[type][rot][i][1];
		if (cx < 0 || cx >= BOARD_W || cy < 0 || cy >= BOARD_H)
			return false;
		if (g.board[cy][cx] != 0)
			return false;
	}
	return true;
}

/** Stamp the active piece into the board as locked cells. */
static void
lock_piece(game_state_t &g)
{
	for (int i = 0; i < 4; i++) {
		int cx = g.cur_x + g_pieces[g.cur_type][g.cur_rot][i][0];
		int cy = g.cur_y + g_pieces[g.cur_type][g.cur_rot][i][1];
		if (cy >= 0 && cy < BOARD_H && cx >= 0 && cx < BOARD_W)
			g.board[cy][cx] = g.cur_type + 1;
	}
}

/**
 * Scan for and remove completed rows; shift everything above down.
 * @return count of lines cleared (0-4).
 */
static int
clear_lines(game_state_t &g)
{
	int cleared = 0;
	for (int row = BOARD_H - 1; row >= 0; row--) {
		bool full = true;
		for (int col = 0; col < BOARD_W; col++) {
			if (g.board[row][col] == 0) {
				full = false;
				break;
			}
		}
		if (full) {
			cleared++;
			for (int r = row; r > 0; r--)
				memcpy(g.board[r], g.board[r - 1], sizeof(g.board[0]));
			memset(g.board[0], 0, sizeof(g.board[0]));
			row++; /* re-check this index after rows shifted down */
		}
	}
	return cleared;
}

/**
 * Lock the active piece, clear lines, update score/level, spawn next.
 * Sets game_over if the spawn position is already occupied.
 * @return number of lines cleared.
 */
static int
do_lock_and_spawn(game_state_t &g)
{
	const int base_score[] = {0, 40, 100, 300, 1200};

	lock_piece(g);
	int n = clear_lines(g);
	if (n > 0) {
		g.score       += base_score[n] * g.level;
		g.total_lines += n;
		int new_level  = g.total_lines / 10 + 1;
		if (new_level > g.level) {
			g.level            = new_level;
			g.drop_interval_ms = drop_interval_for_level(g.level);
		}
	}
	g.cur_type  = g.next_type;
	g.cur_rot   = 0;
	g.cur_x     = BOARD_W / 2 - 2;
	g.cur_y     = 0;
	g.next_type = rand_piece(g);
	if (!piece_fits(g, g.cur_type, g.cur_rot, g.cur_x, g.cur_y))
		g.game_over = true;
	return n;
}

/** Reset all game state and start a fresh game. */
static void
game_init(game_state_t &g)
{
	memset(g.board, 0, sizeof(g.board));
	g.score            = 0;
	g.level            = 1;
	g.total_lines      = 0;
	g.game_over        = false;
	g.paused           = false;
	g.rng_state        = (unsigned)now_ms();
	g.next_type        = rand_piece(g);
	g.drop_interval_ms = drop_interval_for_level(1);
	g.last_drop_ms     = now_ms();
	g.cur_type  = rand_piece(g);
	g.cur_rot   = 0;
	g.cur_x     = BOARD_W / 2 - 2;
	g.cur_y     = 0;
	g.next_type = rand_piece(g);
}

/* ------------------------------------------------------------------ */
/* Input                                                                */
/* ------------------------------------------------------------------ */

/** Handle one key event forwarded by the display server. */
static void
handle_key(game_state_t &g, uint32_t keycode, uint8_t pressed)
{
	if (!pressed)
		return;

	if (keycode == 'p' || keycode == 'P') {
		if (!g.game_over)
			g.paused = !g.paused;
		return;
	}

	/* Track selection: 1/2/3 switch music without pausing the game */
	if (keycode >= '1' && keycode <= '9') {
		int t = (int)(keycode - '1');
		if (t < g_track_count) {
			g.music_track       = t;
			g.music_note        = 0;
			g.music_note_end_ms = now_ms();
		}
		return;
	}

	if (g.paused || g.game_over)
		return;

	switch (keycode) {
	case KEY_LEFT:
		if (piece_fits(g, g.cur_type, g.cur_rot, g.cur_x - 1, g.cur_y)) {
			g.cur_x--;
			sfx_play(g.audio_fd, 280, 25);
		}
		break;

	case KEY_RIGHT:
		if (piece_fits(g, g.cur_type, g.cur_rot, g.cur_x + 1, g.cur_y)) {
			g.cur_x++;
			sfx_play(g.audio_fd, 280, 25);
		}
		break;

	case KEY_DOWN:
		if (piece_fits(g, g.cur_type, g.cur_rot, g.cur_x, g.cur_y + 1)) {
			g.cur_y++;
			g.score++;
			g.last_drop_ms = now_ms();
		}
		break;

	case KEY_UP: {
		/* Rotate clockwise with simple ±1 column wall-kick */
		int new_rot = (g.cur_rot + 1) & 3;
		if (piece_fits(g, g.cur_type, new_rot, g.cur_x, g.cur_y)) {
			g.cur_rot = new_rot;
			sfx_play(g.audio_fd, 440, 35);
		} else if (piece_fits(g, g.cur_type, new_rot, g.cur_x + 1, g.cur_y)) {
			g.cur_x++;
			g.cur_rot = new_rot;
			sfx_play(g.audio_fd, 440, 35);
		} else if (piece_fits(g, g.cur_type, new_rot, g.cur_x - 1, g.cur_y)) {
			g.cur_x--;
			g.cur_rot = new_rot;
			sfx_play(g.audio_fd, 440, 35);
		}
		break;
	}

	case ' ': {
		/* Hard drop: fall to lowest valid row, then lock */
		int rows = 0;
		while (piece_fits(g, g.cur_type, g.cur_rot, g.cur_x, g.cur_y + 1)) {
			g.cur_y++;
			rows++;
		}
		g.score += rows * 2;
		int n    = do_lock_and_spawn(g);
		sfx_play(g.audio_fd, 200 + n * 80, 55);
		g.last_drop_ms = now_ms();
		break;
	}

	default:
		break;
	}
}

/* ------------------------------------------------------------------ */
/* Rendering                                                            */
/* ------------------------------------------------------------------ */

/**
 * Draw one Tetris cell at pixel position (px, py) with a bevel highlight.
 */
static void
draw_cell(ogSurface &surf, int px, int py, uint32_t color)
{
	int x1 = px + 1;
	int y1 = py + 1;
	int x2 = px + CELL_SZ - 2;
	int y2 = py + CELL_SZ - 2;
	surf.ogFillRect(x1, y1, x2, y2, color);
	/* Bright top-left bevel */
	uint32_t r  = ((color >> 16) & 0xFFu) + 0x50u;
	uint32_t gv = ((color >>  8) & 0xFFu) + 0x50u;
	uint32_t bv = ( color        & 0xFFu) + 0x50u;
	uint32_t hi = ((r  > 0xFFu ? 0xFFu : r)  << 16u) |
	              ((gv > 0xFFu ? 0xFFu : gv) <<  8u) |
	               (bv > 0xFFu ? 0xFFu : bv);
	surf.ogFillRect(x1, y1, x2, y1 + 1, hi);
	surf.ogFillRect(x1, y1, x1 + 1, y2, hi);
	/* Dark bottom-right shadow */
	uint32_t sh = (((color >> 16) & 0xFFu) >> 1u) << 16u |
	              (((color >>  8) & 0xFFu) >> 1u) <<  8u |
	              (( color        & 0xFFu) >> 1u);
	surf.ogFillRect(x1, y2 - 1, x2, y2, sh);
	surf.ogFillRect(x2 - 1, y1, x2, y2, sh);
}

/** Find the lowest row the active piece can fall to (ghost display). */
static int
ghost_row(const game_state_t &g)
{
	int gy = g.cur_y;
	while (piece_fits(g, g.cur_type, g.cur_rot, g.cur_x, gy + 1))
		gy++;
	return gy;
}

/** Render the full game frame into the shared-memory surface. */
static void
render(ogSurface &surf, ogBitFont &font, const game_state_t &g,
       int win_w, int win_h)
{
	char buf[24];

	/* Background */
	surf.ogFillRect(0, 0, win_w - 1, win_h - 1, COL_BG);
	surf.ogFillRect(0, 0, BOARD_PX_W - 1, BOARD_PX_H - 1, COL_GRID);

	/* Locked cells */
	for (int row = 0; row < BOARD_H; row++) {
		for (int col = 0; col < BOARD_W; col++) {
			int v = g.board[row][col];
			if (v > 0)
				draw_cell(surf, col * CELL_SZ, row * CELL_SZ,
				          g_piece_colors[v]);
		}
	}

	if (!g.game_over) {
		/* Ghost piece */
		int gy = ghost_row(g);
		if (gy != g.cur_y) {
			for (int i = 0; i < 4; i++) {
				int cx = g.cur_x + g_pieces[g.cur_type][g.cur_rot][i][0];
				int cy = gy      + g_pieces[g.cur_type][g.cur_rot][i][1];
				if (cx >= 0 && cx < BOARD_W && cy >= 0 && cy < BOARD_H)
					draw_cell(surf, cx * CELL_SZ, cy * CELL_SZ,
					          COL_GHOST);
			}
		}
		/* Active piece */
		for (int i = 0; i < 4; i++) {
			int cx = g.cur_x + g_pieces[g.cur_type][g.cur_rot][i][0];
			int cy = g.cur_y + g_pieces[g.cur_type][g.cur_rot][i][1];
			if (cx >= 0 && cx < BOARD_W && cy >= 0 && cy < BOARD_H)
				draw_cell(surf, cx * CELL_SZ, cy * CELL_SZ,
				          g_piece_colors[g.cur_type + 1]);
		}
	}

	/* Board right border */
	surf.ogFillRect(BOARD_PX_W, 0, BOARD_PX_W + 3, BOARD_PX_H - 1,
	                COL_BORDER);

	/* Sidebar */
	surf.ogFillRect(SIDE_X, 0, win_w - 1, win_h - 1, COL_SIDEBAR);

	int sx = SIDE_X + 4;
	int sy = 6;

	font.SetFGColor(0xD0, 0xD0, 0xFF, 255);
	font.SetBGColor(0x14, 0x14, 0x28, 255);
	font.PutString(surf, sx, sy, "TESSERA");
	sy += FONT_H + 4;
	font.PutString(surf, sx, sy, "-------");
	sy += FONT_H + 8;

	font.SetFGColor(0x88, 0x88, 0xCC, 255);
	font.PutString(surf, sx, sy, "SCORE");
	sy += FONT_H + 2;
	font.SetFGColor(0xFF, 0xFF, 0xFF, 255);
	snprintf(buf, sizeof(buf), "%d", g.score);
	font.PutString(surf, sx, sy, buf);
	sy += FONT_H + 8;

	font.SetFGColor(0x88, 0x88, 0xCC, 255);
	font.PutString(surf, sx, sy, "LEVEL");
	sy += FONT_H + 2;
	font.SetFGColor(0xFF, 0xFF, 0xFF, 255);
	snprintf(buf, sizeof(buf), "%d", g.level);
	font.PutString(surf, sx, sy, buf);
	sy += FONT_H + 8;

	font.SetFGColor(0x88, 0x88, 0xCC, 255);
	font.PutString(surf, sx, sy, "LINES");
	sy += FONT_H + 2;
	font.SetFGColor(0xFF, 0xFF, 0xFF, 255);
	snprintf(buf, sizeof(buf), "%d", g.total_lines);
	font.PutString(surf, sx, sy, buf);
	sy += FONT_H + 14;

	/* Next piece preview */
	font.SetFGColor(0x88, 0x88, 0xCC, 255);
	font.SetBGColor(0x14, 0x14, 0x28, 255);
	font.PutString(surf, sx, sy, "NEXT");
	sy += FONT_H + 4;

	const int pcell = 11;
	surf.ogFillRect(sx, sy, sx + pcell * 4 - 1, sy + pcell * 4 - 1,
	                COL_GRID);
	for (int i = 0; i < 4; i++) {
		int cx  = g_pieces[g.next_type][0][i][0];
		int cy  = g_pieces[g.next_type][0][i][1];
		int npx = sx + cx * pcell + 1;
		int npy = sy + cy * pcell + 1;
		surf.ogFillRect(npx, npy, npx + pcell - 3, npy + pcell - 3,
		                g_piece_colors[g.next_type + 1]);
	}
	sy += pcell * 4 + 14;

	/* Current track name */
	font.SetFGColor(0x88, 0x88, 0xCC, 255);
	font.SetBGColor(0x14, 0x14, 0x28, 255);
	font.PutString(surf, sx, sy, "MUSIC");
	sy += FONT_H + 2;
	font.SetFGColor(0xFF, 0xFF, 0xFF, 255);
	font.PutString(surf, sx, sy, g_tracks[g.music_track].name);
	sy += FONT_H + 12;

	/* Key hints */
	font.SetFGColor(0x50, 0x50, 0x80, 255);
	font.PutString(surf, sx, sy, "< >  move");   sy += FONT_H + 3;
	font.PutString(surf, sx, sy, "^    rotate");  sy += FONT_H + 3;
	font.PutString(surf, sx, sy, "v    soft");    sy += FONT_H + 3;
	font.PutString(surf, sx, sy, "SPC  hard");    sy += FONT_H + 3;
	font.PutString(surf, sx, sy, "P    pause");   sy += FONT_H + 3;
	font.PutString(surf, sx, sy, "1-3  music");   sy += FONT_H + 3;
	font.PutString(surf, sx, sy, "Q    quit");

	/* Overlays */
	if (g.paused) {
		font.SetFGColor(0xFF, 0xFF, 0x00, 255);
		font.SetBGColor(0x10, 0x10, 0x20, 255);
		int ox = (BOARD_PX_W - 6 * FONT_W) / 2;
		font.PutString(surf, ox, BOARD_PX_H / 2 - FONT_H, "PAUSED");
	}

	if (g.game_over) {
		font.SetFGColor(0xFF, 0x44, 0x44, 255);
		font.SetBGColor(0x10, 0x10, 0x20, 255);
		int ox = (BOARD_PX_W - 9 * FONT_W) / 2;
		font.PutString(surf, ox, BOARD_PX_H / 2 - FONT_H - 2, "GAME OVER");
		font.SetFGColor(0xA0, 0xA0, 0xFF, 255);
		font.PutString(surf, SIDE_X + 4, BOARD_PX_H / 2 + 4, "SPC=new");
	}
}

/* ------------------------------------------------------------------ */
/* Music                                                                */
/* ------------------------------------------------------------------ */

/**
 * Advance Korobeiniki playback by one note if the current note has elapsed.
 * Writes at most 250 ms of PCM per call to bound latency.
 */
static void
music_tick(game_state_t &g)
{
	if (g.audio_fd < 0)
		return;
	long t = now_ms();
	if (t < g.music_note_end_ms)
		return;

	const track_t      &track = g_tracks[g.music_track];
	const music_note_t &note  = track.notes[g.music_note];
	int ms = (note.ms > 250) ? 250 : note.ms;

	static int16_t g_music_buf[(AUDIO_SR / 4 + 1) * 2]; /* 250 ms stereo */
	int pairs = gen_tone(g_music_buf, AUDIO_SR / 4 + 1, note.freq, ms);
	audio_write(g.audio_fd, g_music_buf, pairs * 4);

	g.music_note_end_ms = t + note.ms;
	g.music_note        = (g.music_note + 1) % track.len;
}

/* ------------------------------------------------------------------ */
/* Entry point                                                          */
/* ------------------------------------------------------------------ */

int // NOLINT(misc-use-internal-linkage)
main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	if (!ubix::views_running()) {
		printf("tessera: views compositor is not running\n");
		return 1;
	}

	ubix::Mailbox mbox;
	mbox.assign("tessera." + std::to_string(ubix::pid()));
	if (!mbox.create())
		return 1;

	const char *views_mb = "views";

	/* Claim a window from the compositor */
	mpi_message_t msg = {};
	struct display_claim_req *creq = (struct display_claim_req *)msg.data;
	msg.header       = DISPLAY_CLAIM;
	creq->x          = 60;
	creq->y          = 40;
	creq->w          = WIN_W;
	creq->h          = WIN_H;
	creq->sender_pid = ubix::pid();
	std::strncpy(creq->title, "Tessera", sizeof(creq->title) - 1);
	std::strncpy(creq->reply, mbox.c_str(), sizeof(creq->reply) - 1);
	ubix::post_message(views_mb, DISPLAY_CLAIM, msg);

	mpi_message_t reply;
	while (!mbox.try_fetch(reply))
		ubix::yield();

	if (reply.header != DISPLAY_ACK)
		return 1;

	struct display_ack *da = (struct display_ack *)reply.data;
	uint32_t win_id = da->window_id;
	void    *shm    = da->shm_base;
	int      act_w  = da->w;
	int      act_h  = da->h;

	if (!shm || act_w <= 0 || act_h <= 0)
		return 1;

	ogSurface surf;
	ogBitFont font;
	if (!surf.ogAttach(shm, (uint32_t)act_w, (uint32_t)act_h, OG_PIXFMT_32BPP))
		return 1;
	font.Load(FONT_PATH, 0);

	game_state_t g = {};
	g.audio_fd = audio_open("/dev/audio");
	game_init(g);
	g.music_track       = 0;
	g.music_note        = 0;
	g.music_note_end_ms = now_ms();

	auto send_flip = [&]() {
		mpi_message_t m = {};
		struct display_flip *fl = (struct display_flip *)m.data;
		m.header      = DISPLAY_FLIP;
		fl->window_id = win_id;
		fl->dirty_x   = 0;
		fl->dirty_y   = 0;
		fl->dirty_w   = act_w;
		fl->dirty_h   = act_h;
		ubix::post_message(views_mb, DISPLAY_FLIP, m);
	};

	auto do_release = [&]() {
		mpi_message_t rel = {};
		struct display_release *dr = (struct display_release *)rel.data;
		rel.header    = DISPLAY_RELEASE;
		dr->window_id = win_id;
		ubix::post_message(views_mb, DISPLAY_RELEASE, rel);
		if (g.audio_fd >= 0)
			audio_close(g.audio_fd);
		mbox.destroy();
	};

	render(surf, font, g, act_w, act_h);
	send_flip();

	for (;;) {
		bool dirty = false;

		/* Poll all pending MPI messages */
		while (mbox.try_fetch(reply)) {
			if (reply.header == DISPLAY_CLOSE) {
				do_release();
				return 0;
			}
			if (reply.header != DISPLAY_KEY)
				continue;

			struct display_key *dk = (struct display_key *)reply.data;

			if ((dk->keycode == 'q' || dk->keycode == 'Q') &&
			    dk->pressed) {
				do_release();
				return 0;
			}

			/* Restart on space when game is over */
			if (g.game_over && dk->keycode == ' ' && dk->pressed) {
				int saved_track = g.music_track;
				game_init(g);
				g.music_track       = saved_track;
				g.music_note        = 0;
				g.music_note_end_ms = now_ms();
				dirty = true;
				continue;
			}

			handle_key(g, dk->keycode, dk->pressed);
			dirty = true;
		}

		/* Gravity */
		if (!g.paused && !g.game_over) {
			long t = now_ms();
			if (t - g.last_drop_ms >= g.drop_interval_ms) {
				g.last_drop_ms = t;
				if (piece_fits(g, g.cur_type, g.cur_rot,
				               g.cur_x, g.cur_y + 1)) {
					g.cur_y++;
				} else {
					int n = do_lock_and_spawn(g);
					sfx_play(g.audio_fd, 220 + n * 110, 60);
				}
				dirty = true;
			}
		}

		music_tick(g);

		if (dirty) {
			render(surf, font, g, act_w, act_h);
			send_flip();
		}

		ubix::yield();
	}

	return 0;
}
