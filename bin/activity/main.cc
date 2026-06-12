/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Activity Monitor — a macOS Activity Monitor-style process viewer.  Reads /proc
 * once a second and renders a live, sortable table of every process: PID, name,
 * parent, state, CPU%, resident memory, and elapsed CPU time.
 *
 * Pure read-/proc + draw-with-objGFX; no new compositor protocol.  CPU% is the
 * delta of each task's run_ticks (/proc/<pid>/stat field 14) over the delta of
 * total busy+idle ticks (/proc/stat) between two 1 Hz samples — HZ-independent.
 * RSS comes from /proc/<pid>/statm; elapsed time is run_ticks scaled to seconds
 * via /proc/uptime.  See docs/design/activity-monitor-plan.md (Layer 3).
 */
extern "C"
{
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <sys/mpi.h>
#include <sys/sched.h>
#include <views/display_proto.h>
/* musl gates nanosleep behind a feature macro the -std=c++20 (__STRICT_ANSI__)
 * -nostdinc world doesn't set; declare it directly, as bin/aural does. */
int nanosleep(const struct timespec *req, struct timespec *rem);
}
#include <objgfx/objgfx.h>
#include <objgfx/ogScalableFont.h>

#define RGB(r, g, b) ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))

#define WIN_W 640
#define WIN_H 460
#define HEADER_H 30
#define ROW_H 22
#define FONT_PATH "/var/fonts/DejaVuSans.ttf"
#define MAX_PROCS 256

/* Palette (light, macOS-ish). */
static const uint32_t COL_WIN = RGB(0xFB, 0xFC, 0xFD);
static const uint32_t COL_HEADER = RGB(0xE9, 0xEC, 0xF0);
static const uint32_t COL_DIVIDER = RGB(0xD2, 0xD6, 0xDC);
static const uint32_t COL_TEXT = RGB(0x20, 0x28, 0x30);
static const uint32_t COL_TEXT_DIM = RGB(0x6B, 0x74, 0x80);
static const uint32_t COL_ROW_ALT = RGB(0xF1, 0xF3, 0xF6);
static const uint32_t COL_CPU_BAR = RGB(0x3B, 0x82, 0xF6);

/* ── columns ────────────────────────────────────────────────────────────────*/
enum
{
	COL_PID = 0,
	COL_NAME,
	COL_PPID,
	COL_STATE,
	COL_CPU,
	COL_RSS,
	COL_TIME,
	NCOLS
};

struct column
{
	const char *title;
	int x;     /* left edge */
	int w;     /* width */
	int right; /* 1 = right-aligned numeric */
};

static struct column g_cols[NCOLS] = {
    {"PID", 10, 56, 1},
    {"Name", 70, 180, 0},
    {"PPID", 254, 56, 1},
    {"State", 316, 56, 0},
    {"CPU%", 376, 80, 1},
    {"RSS", 462, 90, 1},
    {"Time", 558, 72, 1},
};

/* ── process row ────────────────────────────────────────────────────────────*/
struct prow
{
	int pid;
	int ppid;
	char name[40];
	char state;
	unsigned long run_ticks; /* /proc/<pid>/stat field 14 */
	unsigned long rss_pages; /* /proc/<pid>/statm field 2  */
	int cpu_x10;             /* CPU%, fixed point ×10 (computed from deltas) */
};

/* ── state ──────────────────────────────────────────────────────────────────*/
static const char g_mbox[] = "activity";
static const char g_views[] = "views";

static ogSurface g_surf;
static ogScalableFont g_font;
static uint32_t g_win_id;
static int32_t g_w = WIN_W, g_h = WIN_H;

static struct prow g_rows[MAX_PROCS];
static int g_nrows;
static int g_sort = COL_CPU; /* sort column (default: CPU% descending) */

/* Previous-sample bookkeeping for CPU% deltas. */
static unsigned long g_prev_run[MAX_PROCS]; /* indexed by pid (sparse, capped) */
static unsigned long long g_prev_total;     /* prior busy+idle from /proc/stat */
static unsigned long long g_uptime_total;   /* busy+idle now (for Time scaling) */
static unsigned long g_uptime_sec;          /* /proc/uptime first field */

/* ── helpers ────────────────────────────────────────────────────────────────*/

static void set_fg(uint32_t c)
{
	g_font.SetFGColor((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}

/* Read a whole small file into @buf (NUL-terminated).  Returns length or -1. */
static int slurp(const char *path, char *buf, int bufsz)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;
	int n = (int)read(fd, buf, bufsz - 1);
	close(fd);
	if (n < 0)
		n = 0;
	buf[n] = '\0';
	return n;
}

/* Pretty-print @pages of RAM as B/KB/MB into @out. */
static void hsize_pages(unsigned long pages, char *out, int outsz)
{
	unsigned long kb = pages * 4UL; /* PAGE_SIZE 4096 -> 4 KiB/page */
	if (kb < 1024)
		snprintf(out, outsz, "%lu KB", kb);
	else
		snprintf(out, outsz, "%lu.%lu MB", kb / 1024, (kb % 1024) * 10 / 1024);
}

/* run_ticks -> "MM:SS" of CPU time, scaled by the measured tick rate
 * (total_ticks / uptime_sec), so no HZ constant is needed. */
static void fmt_time(unsigned long run_ticks, char *out, int outsz)
{
	unsigned long secs = 0;
	if (g_uptime_total > 0 && g_uptime_sec > 0)
		secs = (unsigned long)((unsigned long long)run_ticks * g_uptime_sec / g_uptime_total);
	snprintf(out, outsz, "%lu:%02lu", secs / 60, secs % 60);
}

/* ── data ───────────────────────────────────────────────────────────────────*/

/* Parse /proc/<pid>/stat: pid (name) state ppid ... utime(field 14). */
static int parse_stat(const char *path, struct prow *r)
{
	char buf[512];
	if (slurp(path, buf, sizeof(buf)) <= 0)
		return -1;

	char *lp = strchr(buf, '(');
	char *rp = strrchr(buf, ')');
	if (!lp || !rp || rp < lp)
		return -1;

	r->pid = atoi(buf);

	int nlen = (int)(rp - lp - 1);
	if (nlen < 0)
		nlen = 0;
	if (nlen > (int)sizeof(r->name) - 1)
		nlen = (int)sizeof(r->name) - 1;
	memcpy(r->name, lp + 1, nlen);
	r->name[nlen] = '\0';

	/* After ')': state ppid pgrp session tty tpgid flags minflt cminflt majflt
	 * cmajflt utime  (ppid is the 1st int, utime the 11th — skip 9 between). */
	char st = '?';
	int ppid = 0, d[9];
	unsigned long ut = 0;
	if (sscanf(rp + 1,
	           " %c %d %d %d %d %d %d %d %d %d %d %lu",
	           &st,
	           &ppid,
	           &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6], &d[7], &d[8],
	           &ut) < 12)
		return -1;
	r->state = st;
	r->ppid = ppid;
	r->run_ticks = ut;
	return 0;
}

/* Read resident pages from /proc/<pid>/statm field 2. */
static unsigned long read_rss(const char *path)
{
	char buf[128];
	unsigned long size = 0, resident = 0;
	if (slurp(path, buf, sizeof(buf)) <= 0)
		return 0;
	sscanf(buf, "%lu %lu", &size, &resident);
	return resident;
}

/* Total busy+idle ticks from /proc/stat's aggregate "cpu" line. */
static unsigned long long read_total_ticks(void)
{
	char buf[256];
	unsigned long long busy = 0, n = 0, s = 0, idle = 0;
	if (slurp("/proc/stat", buf, sizeof(buf)) <= 0)
		return 0;
	/* "cpu  <user> <nice> <system> <idle> ..." — we packed busy into user,
	 * idle into the idle slot. */
	sscanf(buf, "cpu %llu %llu %llu %llu", &busy, &n, &s, &idle);
	return busy + n + s + idle;
}

static int cmp_rows(const void *a, const void *b)
{
	const struct prow *x = (const struct prow *)a;
	const struct prow *y = (const struct prow *)b;
	switch (g_sort)
	{
		case COL_PID:
			return x->pid - y->pid;
		case COL_NAME:
			return strcmp(x->name, y->name);
		case COL_PPID:
			return x->ppid - y->ppid;
		case COL_STATE:
			return (int)x->state - (int)y->state;
		case COL_RSS:
			return (y->rss_pages > x->rss_pages) ? 1 : (y->rss_pages < x->rss_pages) ? -1 : 0;
		case COL_TIME:
			return (y->run_ticks > x->run_ticks) ? 1 : (y->run_ticks < x->run_ticks) ? -1 : 0;
		case COL_CPU:
		default:
			return y->cpu_x10 - x->cpu_x10; /* descending */
	}
}

static void refresh(void)
{
	unsigned long long total = read_total_ticks();
	unsigned long long dtotal = (total > g_prev_total) ? (total - g_prev_total) : 0;

	/* /proc/uptime: "<secs> <idle>" — first field scales Time. */
	{
		char ub[64];
		unsigned long up = 0;
		if (slurp("/proc/uptime", ub, sizeof(ub)) > 0)
			sscanf(ub, "%lu", &up);
		g_uptime_sec = up;
		g_uptime_total = total;
	}

	DIR *d = opendir("/proc");
	if (!d)
		return;

	int n = 0;
	struct dirent *de;
	while ((de = readdir(d)) != 0 && n < MAX_PROCS)
	{
		if (de->d_name[0] < '0' || de->d_name[0] > '9')
			continue; /* only numeric PID dirs */

		char path[64];
		struct prow *r = &g_rows[n];
		snprintf(path, sizeof(path), "/proc/%s/stat", de->d_name);
		if (parse_stat(path, r) != 0)
			continue;
		snprintf(path, sizeof(path), "/proc/%s/statm", de->d_name);
		r->rss_pages = read_rss(path);

		/* CPU% = 100 * Δrun_ticks / Δtotal_ticks (×10 for one decimal). */
		unsigned long prev = (r->pid >= 0 && r->pid < MAX_PROCS) ? g_prev_run[r->pid] : 0;
		unsigned long drun = (r->run_ticks > prev) ? (r->run_ticks - prev) : 0;
		r->cpu_x10 = (dtotal > 0) ? (int)((unsigned long long)drun * 1000 / dtotal) : 0;
		if (r->cpu_x10 > 1000)
			r->cpu_x10 = 1000;
		n++;
	}
	closedir(d);
	g_nrows = n;

	/* Stash this sample's run_ticks for the next delta. */
	memset(g_prev_run, 0, sizeof(g_prev_run));
	for (int i = 0; i < g_nrows; i++)
		if (g_rows[i].pid >= 0 && g_rows[i].pid < MAX_PROCS)
			g_prev_run[g_rows[i].pid] = g_rows[i].run_ticks;
	g_prev_total = total;

	qsort(g_rows, g_nrows, sizeof(g_rows[0]), cmp_rows);
}

/* ── drawing ────────────────────────────────────────────────────────────────*/

/* Draw one cell's text, right- or left-aligned within its column. */
static void cell(const struct column *c, int y, const char *s, uint32_t fg)
{
	set_fg(fg);
	if (c->right)
	{
		int tw = (int)g_font.TextWidth(s);
		g_font.PutString(g_surf, c->x + c->w - tw - 8, y, s);
	}
	else
	{
		g_font.PutString(g_surf, c->x, y, s);
	}
}

static void draw_header(void)
{
	g_surf.ogFillRect(0, 0, g_w - 1, HEADER_H - 1, COL_HEADER);
	g_surf.ogHLine(0, g_w - 1, HEADER_H - 1, COL_DIVIDER);
	for (int i = 0; i < NCOLS; i++)
	{
		char t[24];
		snprintf(t, sizeof(t), "%s%s", g_cols[i].title, (i == g_sort) ? " \xE2\x96\xBE" : "");
		cell(&g_cols[i], 7, t, (i == g_sort) ? COL_TEXT : COL_TEXT_DIM);
	}
}

static void draw_rows(void)
{
	int y = HEADER_H;
	int maxrows = (g_h - HEADER_H) / ROW_H;
	for (int i = 0; i < g_nrows && i < maxrows; i++)
	{
		struct prow *r = &g_rows[i];
		if (i & 1)
			g_surf.ogFillRect(0, y, g_w - 1, y + ROW_H - 1, COL_ROW_ALT);

		/* A faint CPU bar behind the row, proportional to CPU%. */
		if (r->cpu_x10 > 0)
		{
			int bw = (g_cols[COL_CPU].w - 12) * r->cpu_x10 / 1000;
			if (bw > 0)
				g_surf.ogFillRect(g_cols[COL_CPU].x + 4,
				                  y + ROW_H - 4,
				                  g_cols[COL_CPU].x + 4 + bw,
				                  y + ROW_H - 2,
				                  COL_CPU_BAR);
		}

		char buf[48];
		int ty = y + 3;
		snprintf(buf, sizeof(buf), "%d", r->pid);
		cell(&g_cols[COL_PID], ty, buf, COL_TEXT);
		cell(&g_cols[COL_NAME], ty, r->name[0] ? r->name : "?", COL_TEXT);
		snprintf(buf, sizeof(buf), "%d", r->ppid);
		cell(&g_cols[COL_PPID], ty, buf, COL_TEXT_DIM);
		snprintf(buf, sizeof(buf), "%c", r->state);
		cell(&g_cols[COL_STATE], ty, buf, COL_TEXT_DIM);
		snprintf(buf, sizeof(buf), "%d.%d", r->cpu_x10 / 10, r->cpu_x10 % 10);
		cell(&g_cols[COL_CPU], ty, buf, COL_TEXT);
		hsize_pages(r->rss_pages, buf, sizeof(buf));
		cell(&g_cols[COL_RSS], ty, buf, COL_TEXT);
		fmt_time(r->run_ticks, buf, sizeof(buf));
		cell(&g_cols[COL_TIME], ty, buf, COL_TEXT_DIM);
		y += ROW_H;
	}
}

static void flip(void)
{
	mpi_message_t m;
	memset(&m, 0, sizeof(m));
	m.header = DISPLAY_FLIP;
	struct display_flip *fl = (struct display_flip *)m.data;
	fl->window_id = g_win_id;
	fl->dirty_x = 0;
	fl->dirty_y = 0;
	fl->dirty_w = g_w;
	fl->dirty_h = g_h;
	mpi_postMessage((char *)g_views, DISPLAY_FLIP, &m);
}

static void render(void)
{
	g_surf.ogClear(COL_WIN);
	draw_rows();
	draw_header(); /* header last so it stays crisp over row 0 */
	flip();
}

/* ── input ──────────────────────────────────────────────────────────────────*/
static void on_click(int x, int y)
{
	if (y < HEADER_H) /* click a header cell -> sort by that column */
	{
		for (int i = 0; i < NCOLS; i++)
			if (x >= g_cols[i].x && x < g_cols[i].x + g_cols[i].w)
			{
				g_sort = i;
				qsort(g_rows, g_nrows, sizeof(g_rows[0]), cmp_rows);
				render();
				return;
			}
	}
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	mpi_createMbox((char *)g_mbox);

	mpi_message_t msg;
	memset(&msg, 0, sizeof(msg));
	msg.header = DISPLAY_CLAIM;
	struct display_claim_req *req = (struct display_claim_req *)msg.data;
	req->x = 90;
	req->y = 70;
	req->w = WIN_W;
	req->h = WIN_H;
	req->sender_pid = getpid();
	strncpy(req->title, "Activity Monitor", sizeof(req->title) - 1);
	strncpy(req->reply, g_mbox, sizeof(req->reply) - 1);
	req->min_w = 480;
	req->min_h = 280;
	req->max_w = 1000;
	req->max_h = 800;
	while (mpi_postMessage((char *)g_views, DISPLAY_CLAIM, &msg) != 0)
		sched_yield();

	mpi_message_t reply;
	while (mpi_fetchMessage((char *)g_mbox, &reply) != 0)
		sched_yield();
	if (reply.header != DISPLAY_ACK)
		return 1;
	struct display_ack *ack = (struct display_ack *)reply.data;
	g_win_id = ack->window_id;
	g_w = ack->w;
	g_h = ack->h;
	if (!g_surf.ogAttach(ack->shm_base, (uint32_t)g_w, (uint32_t)g_h, OG_PIXFMT_32BPP))
		return 1;
	if (!g_font.Load(FONT_PATH, 13))
		return 1;

	refresh();
	render();

	/* Event loop with a ~1 Hz refresh.  No message -> yield; every ~1 s of idle
	 * polls re-sample /proc and redraw. */
	unsigned poll = 0;
	for (;;)
	{
		mpi_message_t ev;
		if (mpi_fetchMessage((char *)g_mbox, &ev) != 0)
		{
			if (++poll >= 100) /* ~1 s between samples */
			{
				poll = 0;
				refresh();
				render();
			}
			struct timespec ts = {0, 10 * 1000 * 1000}; /* 10 ms; descheduling */
			nanosleep(&ts, 0);
			continue;
		}
		switch (ev.header)
		{
			case DISPLAY_CLOSE:
			{
				mpi_message_t rel;
				memset(&rel, 0, sizeof(rel));
				rel.header = DISPLAY_RELEASE;
				((struct display_release *)rel.data)->window_id = g_win_id;
				mpi_postMessage((char *)g_views, DISPLAY_RELEASE, &rel);
				mpi_destroyMbox((char *)g_mbox);
				return 0;
			}
			case DISPLAY_WINRESIZE:
			{
				struct display_winresize *wr = (struct display_winresize *)ev.data;
				g_w = wr->w;
				g_h = wr->h;
				g_surf.ogAttach(wr->shm_base, (uint32_t)g_w, (uint32_t)g_h, OG_PIXFMT_32BPP);
				render();
				break;
			}
			case DISPLAY_MOUSE:
			{
				struct display_mouse_ev *me = (struct display_mouse_ev *)ev.data;
				if (me->buttons & 1)
					on_click(me->x, me->y);
				break;
			}
			case DISPLAY_KEY:
			{
				struct display_key *k = (struct display_key *)ev.data;
				if (k->pressed && k->keycode == 27) /* ESC closes */
				{
					mpi_message_t rel;
					memset(&rel, 0, sizeof(rel));
					rel.header = DISPLAY_RELEASE;
					((struct display_release *)rel.data)->window_id = g_win_id;
					mpi_postMessage((char *)g_views, DISPLAY_RELEASE, &rel);
					mpi_destroyMbox((char *)g_mbox);
					return 0;
				}
				break;
			}
			default:
				break;
		}
	}
	return 0;
}
