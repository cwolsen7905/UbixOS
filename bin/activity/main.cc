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
	/* musl gates these behind a feature macro the -std=c++20 (__STRICT_ANSI__)
	 * -nostdinc world doesn't set; declare them directly, as bin/aural does. */
	int nanosleep(const struct timespec *req, struct timespec *rem);
	int kill(int pid, int sig);
}
#include <objgfx/objgfx.h>
#include <objgfx/ogScalableFont.h>

#define RGB(r, g, b) ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))

#define WIN_W 640
#define WIN_H 460
#define TABBAR_H 32 /* Windows-style tab strip at the top (Processes/Performance) */
#define HEADER_H 30
#define SUMMARY_H 26 /* overall-CPU strip below the header */
#define ROW_H 22
#define FONT_PATH "/var/fonts/DejaVuSans.ttf"
#define MAX_PROCS 256
#define HIST 24       /* CPU-history samples kept per process (the sparkline) */
#define PERF_HIST 120 /* samples in the Performance-tab graphs (~2 min at 1 Hz) */
#define SIG_TERM 15
#define SIG_KILL 9

/* Top-level views, Windows Task Manager style. */
enum
{
	TAB_PROCESSES = 0,
	TAB_PERFORMANCE,
	NTABS
};

/* Palette (light, macOS-ish). */
static const uint32_t COL_WIN = RGB(0xFB, 0xFC, 0xFD);
static const uint32_t COL_HEADER = RGB(0xE9, 0xEC, 0xF0);
static const uint32_t COL_DIVIDER = RGB(0xD2, 0xD6, 0xDC);
static const uint32_t COL_TEXT = RGB(0x20, 0x28, 0x30);
static const uint32_t COL_TEXT_DIM = RGB(0x6B, 0x74, 0x80);
static const uint32_t COL_ROW_ALT = RGB(0xF1, 0xF3, 0xF6);
static const uint32_t COL_CPU_BAR = RGB(0x3B, 0x82, 0xF6);
static const uint32_t COL_SEL = RGB(0x3B, 0x82, 0xF6);
static const uint32_t COL_SEL_TEXT = RGB(0xFF, 0xFF, 0xFF);
static const uint32_t COL_SPARK = RGB(0xBC, 0xE0, 0xBC); /* faint — text reads over it */

/* Tab strip + Performance-tab graphs (Windows Task Manager palette). */
static const uint32_t COL_TAB_BG = RGB(0xF1, 0xF3, 0xF6);         /* inactive tab strip fill */
static const uint32_t COL_TAB_ACTIVE = RGB(0xFB, 0xFC, 0xFD);     /* active tab (= window bg) */
static const uint32_t COL_TAB_ACCENT = RGB(0x17, 0x9C, 0xE6);     /* active-tab underline      */
static const uint32_t COL_GRAPH_CPU = RGB(0x17, 0x9C, 0xE6);      /* CPU line (Win blue)       */
static const uint32_t COL_GRAPH_CPU_FILL = RGB(0xD3, 0xEC, 0xF8); /* CPU area fill           */
static const uint32_t COL_GRAPH_MEM = RGB(0x90, 0x3D, 0xB0);      /* memory line (Win purple)  */
static const uint32_t COL_GRAPH_MEM_FILL = RGB(0xEA, 0xDC, 0xF1); /* memory area fill        */
static const uint32_t COL_GRID = RGB(0xE2, 0xE6, 0xEB);           /* graph grid lines          */
static const uint32_t COL_GRAPH_BG = RGB(0xFF, 0xFF, 0xFF);       /* graph plot background     */

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
static unsigned long long g_prev_idle;      /* prior idle from /proc/stat       */
static unsigned long long g_uptime_total;   /* busy+idle now (for Time scaling) */
static unsigned long g_uptime_sec;          /* /proc/uptime first field         */

static int g_sel_pid = -1;                    /* selected process (highlight + kill target)    */
static int g_overall_x10;                     /* whole-machine CPU%, ×10                        */
static int g_idle_x10 = 1000;                 /* whole-machine idle%, ×10 (the idle thread)     */
static unsigned char g_hist[MAX_PROCS][HIST]; /* per-pid CPU% ring (0..100)      */
static int g_hist_head;                       /* newest sample index in each ring              */

/* ── Performance tab ─────────────────────────────────────────────────────────*/
static int g_tab = TAB_PROCESSES;           /* active top-level view              */
static unsigned char g_cpu_hist[PERF_HIST]; /* whole-machine CPU% ring (0..100)   */
static unsigned char g_mem_hist[PERF_HIST]; /* memory-used% ring (0..100)         */
static int g_perf_head = -1;                /* newest index in the perf rings     */

/* Per-core CPU graphs (Windows Task Manager "logical processors" view).  Cores
 * come from /proc/stat's cpu0/cpu1/... lines; g_perf_percore toggles between one
 * combined graph (overall) and a grid of one graph per core. */
#define MAX_CORES 8
static unsigned char g_core_hist[MAX_CORES][PERF_HIST]; /* per-core CPU% rings        */
static int g_core_x10[MAX_CORES];                       /* current per-core CPU% (x10) */
static unsigned long long g_prev_core_total[MAX_CORES];
static unsigned long long g_prev_core_idle[MAX_CORES];
static int g_core_count = 1;   /* cpuN lines seen in /proc/stat                       */
static int g_perf_percore = 0; /* 0 = one combined graph, 1 = a graph per core        */
/* Toggle button hit-rect (set in draw_performance, tested in on_click). */
static int g_perf_btn_x, g_perf_btn_y, g_perf_btn_w, g_perf_btn_h;
static unsigned long g_mem_total_kb;        /* /proc/meminfo MemTotal             */
static unsigned long g_mem_used_kb;         /* /proc/meminfo MemUsed              */

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
	           &d[0],
	           &d[1],
	           &d[2],
	           &d[3],
	           &d[4],
	           &d[5],
	           &d[6],
	           &d[7],
	           &d[8],
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

/* Total busy+idle ticks from /proc/stat's aggregate "cpu" line; *idle_out gets
 * the idle slot (for the whole-machine CPU%). */
static unsigned long long read_total_ticks(unsigned long long *idle_out)
{
	char buf[256];
	unsigned long long busy = 0, n = 0, s = 0, idle = 0;
	if (idle_out)
		*idle_out = 0;
	if (slurp("/proc/stat", buf, sizeof(buf)) <= 0)
		return 0;
	/* "cpu  <user> <nice> <system> <idle> ..." — we packed busy into user,
	 * idle into the idle slot. */
	sscanf(buf, "cpu %llu %llu %llu %llu", &busy, &n, &s, &idle);
	if (idle_out)
		*idle_out = idle;
	return busy + n + s + idle;
}

/*
 * Parse /proc/stat's per-core "cpuN <busy> 0 0 <idle> ..." lines into total and
 * idle arrays.  Returns the number of cores seen (the highest cpuN index + 1).
 */
static int read_core_ticks(unsigned long long tot[], unsigned long long idl[])
{
	char buf[512];
	int  count = 0;
	char *p;

	if (slurp("/proc/stat", buf, sizeof(buf)) <= 0)
		return 0;

	for (p = buf; p && *p;)
	{
		/* A per-core line starts "cpu" + a digit (the aggregate is "cpu " + space). */
		if (p[0] == 'c' && p[1] == 'p' && p[2] == 'u' && p[3] >= '0' && p[3] <= '9')
		{
			int idx = -1;
			unsigned long long b = 0, nn = 0, s = 0, id = 0;
			if (sscanf(p, "cpu%d %llu %llu %llu %llu", &idx, &b, &nn, &s, &id) == 5 && idx >= 0 &&
			    idx < MAX_CORES)
			{
				tot[idx] = b + nn + s + id;
				idl[idx] = id;
				if (idx + 1 > count)
					count = idx + 1;
			}
		}
		p = strchr(p, '\n');
		if (p)
			p++;
	}
	return count;
}

/* Read MemTotal/MemUsed (kB) from /proc/meminfo into the g_mem_* globals.  Lines
 * are "MemTotal: <kb> kB" etc.; used := total - free. */
static void read_meminfo(void)
{
	char buf[256];
	unsigned long total = 0, freekb = 0, used = 0;
	char *p;

	if (slurp("/proc/meminfo", buf, sizeof(buf)) <= 0)
		return;
	if ((p = strstr(buf, "MemTotal:")) != NULL)
		total = strtoul(p + 9, NULL, 10);
	if ((p = strstr(buf, "MemFree:")) != NULL)
		freekb = strtoul(p + 8, NULL, 10);
	if ((p = strstr(buf, "MemUsed:")) != NULL)
		used = strtoul(p + 8, NULL, 10);
	else
		used = (total > freekb) ? (total - freekb) : 0;
	g_mem_total_kb = total;
	g_mem_used_kb = used;
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
	unsigned long long idle = 0;
	unsigned long long total = read_total_ticks(&idle);
	unsigned long long dtotal = (total > g_prev_total) ? (total - g_prev_total) : 0;
	unsigned long long didle = (idle > g_prev_idle) ? (idle - g_prev_idle) : 0;

	/* Whole-machine CPU% = busy fraction of the elapsed ticks; idle% is the rest
	 * (the idle thread's ticks, which the kernel buckets separately). */
	g_overall_x10 = (dtotal > 0) ? (int)((dtotal - didle) * 1000 / dtotal) : 0;
	g_idle_x10 = (dtotal > 0) ? (int)(didle * 1000 / dtotal) : 1000;

	/* Advance the history ring once per sample. */
	g_hist_head = (g_hist_head + 1) % HIST;

	/* Performance tab: push this sample's whole-machine CPU% and memory-used%
	 * into the rolling graph rings. */
	read_meminfo();
	g_perf_head = (g_perf_head + 1) % PERF_HIST;
	g_cpu_hist[g_perf_head] = (unsigned char)(g_overall_x10 / 10);
	g_mem_hist[g_perf_head] = (g_mem_total_kb > 0) ? (unsigned char)(g_mem_used_kb * 100 / g_mem_total_kb) : 0;

	/* Per-core CPU%: same busy/(busy+idle) ratio over the elapsed sample, per core,
	 * pushed into each core's ring (shares g_perf_head with the combined graph). */
	{
		unsigned long long ctot[MAX_CORES] = {0}, cidl[MAX_CORES] = {0};
		int nc = read_core_ticks(ctot, cidl);
		if (nc < 1)
			nc = 1;
		if (nc > MAX_CORES)
			nc = MAX_CORES;
		g_core_count = nc;
		for (int c = 0; c < nc; c++)
		{
			unsigned long long dt = (ctot[c] > g_prev_core_total[c]) ? ctot[c] - g_prev_core_total[c] : 0;
			unsigned long long di = (cidl[c] > g_prev_core_idle[c]) ? cidl[c] - g_prev_core_idle[c] : 0;
			g_core_x10[c] = (dt > 0) ? (int)((dt - di) * 1000 / dt) : 0;
			g_prev_core_total[c] = ctot[c];
			g_prev_core_idle[c] = cidl[c];
			g_core_hist[c][g_perf_head] = (unsigned char)(g_core_x10[c] / 10);
		}
	}

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
		if (r->pid >= 0 && r->pid < MAX_PROCS)
			g_hist[r->pid][g_hist_head] = (unsigned char)(r->cpu_x10 / 10); /* 0..100 */
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
	g_prev_idle = idle;

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

#define TAB_W 120 /* width of one tab button in the top strip */

/* Draw the Windows-style tab strip (Processes / Performance) across the top. */
static void draw_tabs(void)
{
	static const char *names[NTABS] = {"Processes", "Performance"};

	g_surf.ogFillRect(0, 0, g_w - 1, TABBAR_H - 1, COL_TAB_BG);
	for (int i = 0; i < NTABS; i++)
	{
		int x0 = i * TAB_W;
		int x1 = x0 + TAB_W - 1;
		bool act = (i == g_tab);
		if (act)
		{
			g_surf.ogFillRect(x0, 0, x1, TABBAR_H - 1, COL_TAB_ACTIVE);
			g_surf.ogFillRect(x0, TABBAR_H - 3, x1, TABBAR_H - 1, COL_TAB_ACCENT);
		}
		set_fg(act ? COL_TEXT : COL_TEXT_DIM);
		int tw = (int)g_font.TextWidth(names[i]);
		g_font.PutString(g_surf, x0 + (TAB_W - tw) / 2, 9, names[i]);
	}
	g_surf.ogHLine(0, g_w - 1, TABBAR_H - 1, COL_DIVIDER);
}

/* Draw a filled-area line graph of a 0..100 ring (oldest at left, newest at the
 * right edge so the trace scrolls in from the right, Windows-style).  @hist is a
 * PERF_HIST ring whose newest sample is at g_perf_head. */
static void draw_graph(int gx, int gy, int gw, int gh, const unsigned char *hist, uint32_t line, uint32_t fill)
{
	int base = gy + gh - 1;
	int prevy = base;

	if (gw < 2 || gh < 2)
		return;

	g_surf.ogFillRect(gx, gy, gx + gw - 1, gy + gh - 1, COL_GRAPH_BG);
	for (int k = 1; k < 4; k++) /* horizontal grid at 25/50/75% */
		g_surf.ogHLine(gx, gx + gw - 1, gy + gh * k / 4, COL_GRID);
	for (int k = 1; k < 8; k++) /* vertical grid */
		g_surf.ogVLine(gx + gw * k / 8, gy, gy + gh - 1, COL_GRID);

	/* One vertical fill bar per pixel column; line segment over the top. */
	for (int px = 0; px < gw; px++)
	{
		int j = px * (PERF_HIST - 1) / (gw - 1);
		int v = hist[(g_perf_head + 1 + j) % PERF_HIST]; /* 0..100 */
		int x = gx + px;
		int y = base - v * (gh - 2) / 100;
		g_surf.ogVLine(x, y, base, fill);
		if (px > 0)
			g_surf.ogLine(x - 1, prevy, x, y, line);
		prevy = y;
	}
	g_surf.ogRect(gx, gy, gx + gw - 1, gy + gh - 1, COL_DIVIDER);
}

/* Draw the View toggle button (Overall <-> Per core) and record its hit-rect. */
static void draw_perf_toggle(int x, int y)
{
	const char *label = g_perf_percore ? "View: Per core" : "View: Overall";
	int tw = (int)g_font.TextWidth(label);
	int bw = tw + 20, bh = 22;
	int bx = x, by = y;

	g_perf_btn_x = bx;
	g_perf_btn_y = by;
	g_perf_btn_w = bw;
	g_perf_btn_h = bh;

	g_surf.ogFillRect(bx, by, bx + bw - 1, by + bh - 1, COL_HEADER);
	g_surf.ogRect(bx, by, bx + bw - 1, by + bh - 1, COL_DIVIDER);
	set_fg(COL_TEXT);
	g_font.PutString(g_surf, bx + 10, by + 4, (char *)label);
}

/* Performance tab: CPU (combined or per-core grid) over Memory, with a heading,
 * readout, and a Windows-style "logical processors" toggle. */
static void draw_performance(void)
{
	const int pad = 16;
	int top = TABBAR_H;
	int x = pad;
	int w = g_w - 2 * pad;
	int sect_h = (g_h - top - pad) / 2;
	int gh = sect_h - 28 - pad;
	char buf[64];
	int tw;

	g_surf.ogFillRect(0, top, g_w - 1, g_h - 1, COL_WIN);

	int cy = top + pad;

	/* View toggle, top-right of the CPU section. */
	{
		const char *l = g_perf_percore ? "View: Per core" : "View: Overall";
		int bw = (int)g_font.TextWidth(l) + 20;
		draw_perf_toggle(x + w - bw, cy - 4);
	}

	if (!g_perf_percore)
	{
		/* One combined CPU graph. */
		set_fg(COL_TEXT);
		g_font.PutString(g_surf, x, cy, "CPU");
		snprintf(buf, sizeof(buf), "%d.%d%% utilization", g_overall_x10 / 10, g_overall_x10 % 10);
		set_fg(COL_TEXT_DIM);
		tw = (int)g_font.TextWidth(buf);
		g_font.PutString(g_surf, x + w - tw - g_perf_btn_w - 12, cy, buf);
		draw_graph(x, cy + 24, w, gh, g_cpu_hist, COL_GRAPH_CPU, COL_GRAPH_CPU_FILL);
	}
	else
	{
		/* A graph per logical processor, laid out in a grid (up to 4 per row). */
		int n = g_core_count;
		if (n < 1)
			n = 1;
		if (n > MAX_CORES)
			n = MAX_CORES;
		int cols = (n >= 4) ? 4 : n;
		int rows = (n + cols - 1) / cols;
		int gap = 8;
		int label_h = 18;
		int cell_w = (w - (cols - 1) * gap) / cols;
		int grid_top = cy + 22;
		int grid_h = sect_h - 22 - pad;
		int cell_h = (grid_h - (rows - 1) * gap) / rows;

		set_fg(COL_TEXT);
		g_font.PutString(g_surf, x, cy, "CPU - per logical processor");

		for (int c = 0; c < n; c++)
		{
			int r = c / cols, k = c % cols;
			int gx = x + k * (cell_w + gap);
			int gy = grid_top + r * (cell_h + gap);
			snprintf(buf, sizeof(buf), "CPU %d   %d%%", c, g_core_x10[c] / 10);
			set_fg(COL_TEXT_DIM);
			g_font.PutString(g_surf, gx, gy, buf);
			draw_graph(gx, gy + label_h, cell_w, cell_h - label_h, g_core_hist[c], COL_GRAPH_CPU,
			           COL_GRAPH_CPU_FILL);
		}
	}

	/* Memory (bottom half). */
	int my = top + sect_h + pad;
	set_fg(COL_TEXT);
	g_font.PutString(g_surf, x, my, "Memory");
	snprintf(buf,
	         sizeof(buf),
	         "%lu / %lu MB  (%d%%)",
	         g_mem_used_kb / 1024,
	         g_mem_total_kb / 1024,
	         g_mem_total_kb ? (int)(g_mem_used_kb * 100 / g_mem_total_kb) : 0);
	set_fg(COL_TEXT_DIM);
	tw = (int)g_font.TextWidth(buf);
	g_font.PutString(g_surf, x + w - tw, my, buf);
	draw_graph(x, my + 24, w, gh, g_mem_hist, COL_GRAPH_MEM, COL_GRAPH_MEM_FILL);
}

static void draw_header(void)
{
	g_surf.ogFillRect(0, TABBAR_H, g_w - 1, TABBAR_H + HEADER_H - 1, COL_HEADER);
	g_surf.ogHLine(0, g_w - 1, TABBAR_H + HEADER_H - 1, COL_DIVIDER);
	for (int i = 0; i < NCOLS; i++)
	{
		char t[24];
		/* Sort indicator: ASCII caret (not a UTF-8 glyph).  ogScalableFont walks
		 * bytes, not codepoints, so a multi-byte arrow renders as mojibake until
		 * the font engine learns UTF-8 (queued with [ls/objgfx]). */
		snprintf(t, sizeof(t), "%s%s", g_cols[i].title, (i == g_sort) ? " v" : "");
		cell(&g_cols[i], TABBAR_H + 7, t, (i == g_sort) ? COL_TEXT : COL_TEXT_DIM);
	}
}

/* Whole-machine CPU strip below the header: a label + a usage bar. */
static void draw_summary(void)
{
	int y0 = TABBAR_H + HEADER_H;
	char buf[32];
	int bx, bw, by, bh, fill;

	g_surf.ogFillRect(0, y0, g_w - 1, y0 + SUMMARY_H - 1, COL_WIN);
	g_surf.ogHLine(0, g_w - 1, y0 + SUMMARY_H - 1, COL_DIVIDER);

	snprintf(buf,
	         sizeof(buf),
	         "CPU %d.%d%%   Idle %d.%d%%",
	         g_overall_x10 / 10,
	         g_overall_x10 % 10,
	         g_idle_x10 / 10,
	         g_idle_x10 % 10);
	set_fg(COL_TEXT);
	g_font.PutString(g_surf, 10, y0 + 5, buf);

	bx = 240;
	bw = g_w - bx - 14;
	by = y0 + 6;
	bh = SUMMARY_H - 13;
	g_surf.ogFillRect(bx, by, bx + bw, by + bh, COL_ROW_ALT);
	fill = bw * g_overall_x10 / 1000;
	if (fill > 0)
		g_surf.ogFillRect(bx, by, bx + fill, by + bh, COL_CPU_BAR);
	g_surf.ogRect(bx, by, bx + bw, by + bh, COL_DIVIDER);
}

/* A faint CPU-history sparkbar behind the CPU% cell of row at @y for @r. */
static void draw_sparkline(struct prow *r, int y)
{
	int cx = g_cols[COL_CPU].x + 2;
	int cw = g_cols[COL_CPU].w - 6;
	int base = y + ROW_H - 3;
	int hmax = ROW_H - 7;
	int bw = cw / HIST > 1 ? cw / HIST - 1 : 1;

	if (r->pid < 0 || r->pid >= MAX_PROCS)
		return;
	for (int j = 0; j < HIST; j++)
	{
		int idx = (g_hist_head + 1 + j) % HIST;
		int v = g_hist[r->pid][idx]; /* 0..100 */
		int bh = v * hmax / 100;
		int sx = cx + j * cw / HIST;
		if (bh > 0)
			g_surf.ogFillRect(sx, base - bh, sx + bw, base, COL_SPARK);
	}
}

static void draw_rows(void)
{
	int top = TABBAR_H + HEADER_H + SUMMARY_H;
	int y = top;
	int maxrows = (g_h - top) / ROW_H;
	for (int i = 0; i < g_nrows && i < maxrows; i++)
	{
		struct prow *r = &g_rows[i];
		bool sel = (r->pid == g_sel_pid);
		uint32_t fg = sel ? COL_SEL_TEXT : COL_TEXT;
		uint32_t fgd = sel ? COL_SEL_TEXT : COL_TEXT_DIM;
		char buf[48];
		int ty = y + 3;

		if (sel)
			g_surf.ogFillRect(0, y, g_w - 1, y + ROW_H - 1, COL_SEL);
		else if (i & 1)
			g_surf.ogFillRect(0, y, g_w - 1, y + ROW_H - 1, COL_ROW_ALT);

		if (!sel)
			draw_sparkline(r, y); /* history behind the CPU% number */

		snprintf(buf, sizeof(buf), "%d", r->pid);
		cell(&g_cols[COL_PID], ty, buf, fg);
		cell(&g_cols[COL_NAME], ty, r->name[0] ? r->name : "?", fg);
		snprintf(buf, sizeof(buf), "%d", r->ppid);
		cell(&g_cols[COL_PPID], ty, buf, fgd);
		snprintf(buf, sizeof(buf), "%c", r->state);
		cell(&g_cols[COL_STATE], ty, buf, fgd);
		snprintf(buf, sizeof(buf), "%d.%d", r->cpu_x10 / 10, r->cpu_x10 % 10);
		cell(&g_cols[COL_CPU], ty, buf, fg);
		hsize_pages(r->rss_pages, buf, sizeof(buf));
		cell(&g_cols[COL_RSS], ty, buf, fg);
		fmt_time(r->run_ticks, buf, sizeof(buf));
		cell(&g_cols[COL_TIME], ty, buf, fgd);
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
	if (g_tab == TAB_PERFORMANCE)
	{
		draw_performance();
	}
	else
	{
		draw_rows();
		draw_summary();
		draw_header(); /* header last so it stays crisp over row 0 */
	}
	draw_tabs(); /* tab strip on top, both views */
	flip();
}

/* ── input ──────────────────────────────────────────────────────────────────*/
static void on_click(int x, int y)
{
	if (y < TABBAR_H) /* click a tab -> switch view */
	{
		int t = x / TAB_W;
		if (t >= 0 && t < NTABS && t != g_tab)
		{
			g_tab = t;
			render();
		}
		return;
	}

	if (g_tab == TAB_PERFORMANCE)
	{
		/* Toggle the CPU view between one combined graph and per-core graphs. */
		if (x >= g_perf_btn_x && x < g_perf_btn_x + g_perf_btn_w && y >= g_perf_btn_y &&
		    y < g_perf_btn_y + g_perf_btn_h)
		{
			g_perf_percore = !g_perf_percore;
			render();
		}
		return;
	}

	if (g_tab != TAB_PROCESSES)
		return;

	if (y < TABBAR_H + HEADER_H) /* click a header cell -> sort by that column */
	{
		for (int i = 0; i < NCOLS; i++)
			if (x >= g_cols[i].x && x < g_cols[i].x + g_cols[i].w)
			{
				g_sort = i;
				qsort(g_rows, g_nrows, sizeof(g_rows[0]), cmp_rows);
				render();
				return;
			}
		return;
	}

	/* Click a row -> select it (the force-quit target). */
	int top = TABBAR_H + HEADER_H + SUMMARY_H;
	if (y >= top)
	{
		int i = (y - top) / ROW_H;
		if (i >= 0 && i < g_nrows)
		{
			g_sel_pid = g_rows[i].pid;
			render();
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

	/* Event loop: block on the mailbox with a ~1 s timeout.  A client message
	 * (mouse/key/resize/close) wakes us instantly; on timeout we re-sample /proc
	 * and redraw.  Blocking via mpi_waitMessage instead of a nanosleep poll keeps
	 * an idle Task Manager off the CPU — critical under i386/TCG, where a busy
	 * spin steals emulation throughput from the rest of the system. */
	for (;;)
	{
		mpi_message_t ev;
		if (mpi_waitMessage((char *)g_mbox, &ev, 100) != 0) /* 100 ticks ~= 1 s */
		{
			refresh();
			render();
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
				/* Force-quit the selected process: 'k' = SIGTERM, 'K' = SIGKILL. */
				if (k->pressed && g_sel_pid > 1 && (k->keycode == 'k' || k->keycode == 'K'))
				{
					kill(g_sel_pid, (k->keycode == 'K') ? SIG_KILL : SIG_TERM);
					refresh();
					render();
				}
				break;
			}
			default:
				break;
		}
	}
	return 0;
}
