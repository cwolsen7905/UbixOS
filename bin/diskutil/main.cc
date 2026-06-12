/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Disk Utility — a graphical block-device browser (macOS Disk Utility-inspired):
 * a sidebar tree of drives + partitions, a detail panel with a coloured
 * partition-layout bar + an info grid + a used/free capacity bar, and a toolbar.
 *
 * Phase 1 (MVP) is read-only: it enumerates via ubix_disk_query (syscall 68) and
 * reports used/free for UbixFS volumes via ubix_pool_query (67).  The toolbar
 * actions (Mount/Erase/Partition/First Aid) are stubs for later phases.  All
 * visual elements are reusable objGFX widgets (ogSegmentBar/ogButton), not
 * app-private.  See docs/design/disk-utility-plan.md.
 */
extern "C"
{
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mpi.h>
#include <sys/sched.h>
#include <views/display_proto.h>
#include <api/ubix_disk.h>
#include <api/ubfs_pool.h>
}
#include <objgfx/objgfx.h>
#include <objgfx/ogScalableFont.h>
#include <objgfx/ogSegmentBar.h>
#include <objgfx/ogButton.h>

#define RGB(r, g, b) ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))

#define WIN_W 720
#define WIN_H 480
#define TOOLBAR_H 44
#define SIDEBAR_W 210
#define ROW_H 26
#define FONT_PATH "/var/fonts/DejaVuSans.ttf"

/* Palette. */
static const uint32_t COL_WIN = RGB(0xF2, 0xF3, 0xF5);
static const uint32_t COL_TOOLBAR = RGB(0xE6, 0xE8, 0xEC);
static const uint32_t COL_SIDEBAR = RGB(0xEC, 0xEE, 0xF1);
static const uint32_t COL_SEL = RGB(0x3B, 0x82, 0xF6);
static const uint32_t COL_DIVIDER = RGB(0xC8, 0xCC, 0xD2);
static const uint32_t COL_TEXT = RGB(0x20, 0x28, 0x30);
static const uint32_t COL_TEXT_DIM = RGB(0x70, 0x78, 0x82);
static const uint32_t COL_SEL_TEXT = RGB(0xFF, 0xFF, 0xFF);

/* Partition fill colours by MBR type. */
static uint32_t type_color(uint8_t t)
{
	switch (t)
	{
		case UBIX_PT_FAT32:
			return RGB(0x4A, 0x90, 0xD9); /* blue */
		case UBIX_PT_SWAP:
			return RGB(0x90, 0x98, 0xA4); /* grey */
		case UBIX_PT_UBPOOL:
			return RGB(0x4C, 0xAF, 0x50); /* green */
		default:
			return RGB(0xE0, 0x91, 0x3A); /* orange */
	}
}

/* ── state ──────────────────────────────────────────────────────────────────*/
static const char g_mbox[] = "diskutil";
static const char g_views[] = "views";

static ogSurface g_surf;
static ogScalableFont g_font;
static uint32_t g_win_id;
static int32_t g_w = WIN_W, g_h = WIN_H;

static struct ubix_disk_info g_disks[UBIX_DISK_MAX];
static int g_ndisks;
static int g_sel; /* selected entry index into g_disks */

static struct ubix_pool_info g_pools[16];
static int g_npools;

static ogButton g_btn[5];
static const char *g_btn_label[5] = {"Info", "Mount", "Erase", "Partition", "First Aid"};

/* ── helpers ────────────────────────────────────────────────────────────────*/

/* Human-readable size of @sectors (512-byte). */
static void hsize(uint64_t sectors, char *out, size_t outsz)
{
	static const char unit[] = "BKMGT";
	uint64_t bytes = sectors * 512ULL, div = 1;
	int i = 0;
	while (bytes / div >= 1024 && i < 4)
	{
		div *= 1024;
		i++;
	}
	if (i == 0)
		snprintf(out, outsz, "%llu B", (unsigned long long)bytes);
	else
		snprintf(out,
		         outsz,
		         "%llu.%llu %cB",
		         (unsigned long long)(bytes / div),
		         (unsigned long long)((bytes % div) * 10 / div),
		         unit[i]);
}

static const char *type_text(const struct ubix_disk_info *e)
{
	if (e->fstype[0])
		return e->fstype;
	switch (e->mbr_type)
	{
		case UBIX_PT_FAT32:
			return "FAT32";
		case UBIX_PT_SWAP:
			return "Linux swap";
		case UBIX_PT_UBPOOL:
			return "UbixFS pool";
		default:
			return "data";
	}
}

/* The disk entry that owns entry @i (itself if it is a disk). */
static int owning_disk(int i)
{
	if (g_disks[i].kind == UBIX_DK_DISK)
		return i;
	return g_disks[i].parent;
}

/* Find the pool record for a mounted partition (matches by mountpoint). */
static struct ubix_pool_info *pool_for(const struct ubix_disk_info *e)
{
	for (int i = 0; i < g_npools; i++)
		if (e->mountpoint[0] && strcmp(g_pools[i].mountpoint, e->mountpoint) == 0)
			return &g_pools[i];
	return 0;
}

static void set_fg(uint32_t c)
{
	g_font.SetFGColor((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}

/* ── data ───────────────────────────────────────────────────────────────────*/
static void refresh(void)
{
	g_ndisks = ubix_disk_query(g_disks, UBIX_DISK_MAX);
	if (g_ndisks < 0)
		g_ndisks = 0;
	g_npools = ubix_pool_query(g_pools, 16);
	if (g_npools < 0)
		g_npools = 0;
	if (g_sel >= g_ndisks)
		g_sel = 0;
}

/* ── drawing ────────────────────────────────────────────────────────────────*/

static void draw_toolbar(void)
{
	g_surf.ogFillRect(0, 0, g_w - 1, TOOLBAR_H - 1, COL_TOOLBAR);
	g_surf.ogHLine(0, g_w - 1, TOOLBAR_H - 1, COL_DIVIDER);
	int bx = 8;
	for (int i = 0; i < 5; i++)
	{
		int bw = (int)g_font.TextWidth(g_btn_label[i]) + 22;
		g_btn[i].x = bx;
		g_btn[i].y = 7;
		g_btn[i].w = bw;
		g_btn[i].h = TOOLBAR_H - 14;
		g_btn[i].label = g_btn_label[i];
		g_btn[i].enabled = (i == 0); /* MVP: only Info is live */
		g_btn[i].Draw(g_surf, g_font);
		bx += bw + 6;
	}
}

static void draw_sidebar(void)
{
	g_surf.ogFillRect(0, TOOLBAR_H, SIDEBAR_W - 1, g_h - 1, COL_SIDEBAR);
	g_surf.ogVLine(SIDEBAR_W - 1, TOOLBAR_H, g_h - 1, COL_DIVIDER);

	int y = TOOLBAR_H + 6;
	for (int i = 0; i < g_ndisks; i++)
	{
		struct ubix_disk_info *e = &g_disks[i];
		bool sel = (i == g_sel);
		if (sel)
			g_surf.ogFillRect(3, y, SIDEBAR_W - 4, y + ROW_H - 2, COL_SEL);

		int indent = (e->kind == UBIX_DK_PART) ? 26 : 10;
		set_fg(sel ? COL_SEL_TEXT : COL_TEXT);
		g_font.PutString(g_surf, indent, y + 4, e->name);

		/* Mount/type hint on the right for partitions. */
		if (e->kind == UBIX_DK_PART)
		{
			const char *hint = e->mountpoint[0] ? e->mountpoint : type_text(e);
			uint32_t tw = g_font.TextWidth(hint);
			set_fg(sel ? COL_SEL_TEXT : COL_TEXT_DIM);
			g_font.PutString(g_surf, SIDEBAR_W - 8 - (int)tw, y + 4, hint);
		}
		y += ROW_H;
	}
}

static void draw_detail(void)
{
	int dx = SIDEBAR_W + 18;
	int dw = g_w - dx - 18;
	struct ubix_disk_info *e = &g_disks[g_sel];
	struct ubix_disk_info *disk = &g_disks[owning_disk(g_sel)];
	char sz[24];

	g_surf.ogFillRect(SIDEBAR_W, TOOLBAR_H, g_w - 1, g_h - 1, COL_WIN);

	/* Header. */
	hsize(e->sectors, sz, sizeof(sz));
	set_fg(COL_TEXT);
	g_font.PutString(g_surf, dx, TOOLBAR_H + 12, e->name);
	char head[96];
	snprintf(head, sizeof(head), "%s  -  %s  -  %s", sz, type_text(e), disk->model[0] ? disk->model : "MBR");
	set_fg(COL_TEXT_DIM);
	g_font.PutString(g_surf, dx, TOOLBAR_H + 12 + (int)g_font.GetHeight() + 4, head);

	/* Partition-layout bar for the owning disk. */
	int bary = TOOLBAR_H + 64;
	int barh = 34;
	struct ogBarSegment segs[UBIX_DISK_MAX];
	int nseg = 0;
	char seglbl[UBIX_DISK_MAX][40];
	for (int i = 0; i < g_ndisks && nseg < UBIX_DISK_MAX; i++)
	{
		if (g_disks[i].kind != UBIX_DK_PART)
			continue;
		if (owning_disk(i) != owning_disk(g_sel))
			continue;
		char psz[16];
		hsize(g_disks[i].sectors, psz, sizeof(psz));
		snprintf(seglbl[nseg], sizeof(seglbl[nseg]), "%s %s", psz, type_text(&g_disks[i]));
		segs[nseg].weight = g_disks[i].sectors ? g_disks[i].sectors : 1;
		segs[nseg].color = type_color(g_disks[i].mbr_type);
		segs[nseg].label = g_disks[i].name;
		segs[nseg].detail = seglbl[nseg];
		nseg++;
	}
	if (nseg > 0)
		ogSegmentBar::Draw(
		    g_surf, g_font, dx, bary, dw, barh, segs, nseg, COL_DIVIDER, COL_SEL_TEXT, COL_TEXT_DIM);

	/* Info grid. */
	int gy = bary + barh + 28;
	int lh = (int)g_font.GetHeight() + 8;
	struct
	{
		const char *k;
		char v[80];
	} rows[6];
	int nr = 0;
	snprintf(rows[nr].v, sizeof(rows[nr].v), "%s", e->name);
	rows[nr++].k = "Name";
	snprintf(rows[nr].v, sizeof(rows[nr].v), "%s", type_text(e));
	rows[nr++].k = "Type";
	hsize(e->sectors, rows[nr].v, sizeof(rows[nr].v));
	rows[nr++].k = "Capacity";

	struct ubix_pool_info *pi = pool_for(e);
	if (pi)
	{
		uint64_t total = pi->total_blocks * pi->block_size;
		uint64_t freeb = pi->free_blocks * pi->block_size;
		snprintf(rows[nr].v,
		         sizeof(rows[nr].v),
		         "%llu MB free of %llu MB",
		         (unsigned long long)(freeb / 1048576ULL),
		         (unsigned long long)(total / 1048576ULL));
		rows[nr++].k = "Used / Free";
	}
	snprintf(rows[nr].v, sizeof(rows[nr].v), "%s", e->mountpoint[0] ? e->mountpoint : "(not mounted)");
	rows[nr++].k = "Mount point";
	snprintf(rows[nr].v, sizeof(rows[nr].v), "Master Boot Record");
	rows[nr++].k = "Partition map";

	for (int i = 0; i < nr; i++)
	{
		set_fg(COL_TEXT_DIM);
		g_font.PutString(g_surf, dx, gy, rows[i].k);
		set_fg(COL_TEXT);
		g_font.PutString(g_surf, dx + 130, gy, rows[i].v);
		gy += lh;
	}

	/* Used/free capacity bar for a pool volume. */
	if (pi)
	{
		uint64_t tots = (uint64_t)pi->total_blocks * pi->block_size / 512;
		uint64_t frees = (uint64_t)pi->free_blocks * pi->block_size / 512;
		uint64_t useds = (tots > frees) ? (tots - frees) : 0;
		struct ogBarSegment cap[2];
		char ulbl[24], flbl[24];
		hsize(useds, ulbl, sizeof(ulbl));
		hsize(frees, flbl, sizeof(flbl));
		cap[0].weight = useds ? useds : 1;
		cap[0].color = COL_SEL;
		cap[0].label = "Used";
		cap[0].detail = ulbl;
		cap[1].weight = frees ? frees : 1;
		cap[1].color = RGB(0xD8, 0xDC, 0xE2);
		cap[1].label = "Free";
		cap[1].detail = flbl;
		ogSegmentBar::Draw(g_surf, g_font, dx, gy + 8, dw, 22, cap, 2, COL_DIVIDER, COL_TEXT, COL_TEXT_DIM);
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
	draw_toolbar();
	draw_sidebar();
	draw_detail();
	flip();
}

/* ── input ──────────────────────────────────────────────────────────────────*/
static void on_click(int x, int y)
{
	/* Sidebar selection. */
	if (x < SIDEBAR_W && y >= TOOLBAR_H + 6)
	{
		int i = (y - (TOOLBAR_H + 6)) / ROW_H;
		if (i >= 0 && i < g_ndisks && i != g_sel)
		{
			g_sel = i;
			render();
		}
		return;
	}
	/* Toolbar (only Info live in the MVP — it just refreshes). */
	if (y < TOOLBAR_H)
	{
		if (g_btn[0].Hit(x, y))
		{
			refresh();
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
	req->x = 70;
	req->y = 60;
	req->w = WIN_W;
	req->h = WIN_H;
	req->sender_pid = getpid();
	strncpy(req->title, "Disk Utility", sizeof(req->title) - 1);
	strncpy(req->reply, g_mbox, sizeof(req->reply) - 1);
	req->min_w = 560;
	req->min_h = 360;
	req->max_w = 1100;
	req->max_h = 760;
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
	if (!g_font.Load(FONT_PATH, 14))
		return 1;

	refresh();
	render();

	for (;;)
	{
		mpi_message_t ev;
		if (mpi_fetchMessage((char *)g_mbox, &ev) != 0)
		{
			sched_yield();
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
				if (me->buttons & 1) /* act on press */
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
