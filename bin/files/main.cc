/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Files — the uBixOS graphical file manager (P0 MVP).
 *
 * An Explorer-leaning, best-of-all-worlds file browser: a toolbar
 * (back / forward / up / New Folder / Rename / Delete), a breadcrumb address
 * bar, a sortable Details list (Name / Size / Type / Modified), and a status
 * bar.  Double-click a folder to enter it, a file to open it in the app the
 * ubistry registry maps from its extension.  This is a standard views + objGFX
 * client (one process, a shared-memory window buffer, MPI for window control),
 * built the same way as bin/diskutil and bin/activity.
 *
 * It is a pure userland convenience layer over the same VFS the shell uses:
 * browse via opendir/readdir/stat, mutate via mkdir/rmdir/unlink/rename, open
 * via execve of a ubistry-mapped handler.  No new syscalls.  Phase roadmap and
 * rationale: docs/design/file-manager-plan.md.
 */
extern "C"
{
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mpi.h>
#include <sys/sched.h>
#include <sys/kbd.h>
#include <views/display_proto.h>
#include <ubistry/ubistry.h>

	extern char **environ; /* inherited session env, forwarded to opened apps */
}
#include <objgfx/objgfx.h>
#include <objgfx/ogScalableFont.h>
#include <objgfx/ogButton.h>

#define RGB(r, g, b) ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))

#define WIN_W 760
#define WIN_H 500
#define TOOLBAR_H 44
#define ADDR_H 32 /* breadcrumb address bar */
#define HEAD_H 26 /* column-header row */
#define ROW_H 24  /* one list entry */
#define STATUS_H 26
#define PAD 10
#define ICON_W 18
#define FONT_PATH "/var/fonts/DejaVuSans.ttf"

#define MAX_ENTRIES 1024
#define MAX_CRUMBS 64
#define NAME_MAX_LEN 256

/* Palette — matches the diskutil / activity desktop look. */
static const uint32_t COL_WIN = RGB(0xF7, 0xF8, 0xFA);
static const uint32_t COL_TOOLBAR = RGB(0xE9, 0xEB, 0xEF);
static const uint32_t COL_ADDR = RGB(0xFF, 0xFF, 0xFF);
static const uint32_t COL_HEAD = RGB(0xEE, 0xF0, 0xF3);
static const uint32_t COL_SEL = RGB(0x3B, 0x82, 0xF6);
static const uint32_t COL_SEL_TEXT = RGB(0xFF, 0xFF, 0xFF);
static const uint32_t COL_ROW_ALT = RGB(0xF1, 0xF3, 0xF6);
static const uint32_t COL_DIVIDER = RGB(0xD2, 0xD6, 0xDC);
static const uint32_t COL_TEXT = RGB(0x1E, 0x26, 0x30);
static const uint32_t COL_TEXT_DIM = RGB(0x6C, 0x74, 0x80);
static const uint32_t COL_FOLDER = RGB(0x54, 0x9D, 0xE0);
static const uint32_t COL_FILE = RGB(0xB6, 0xBD, 0xC6);
static const uint32_t COL_FILE_FOLD = RGB(0x90, 0x98, 0xA2);
static const uint32_t COL_CRUMB = RGB(0x33, 0x6F, 0xC4);

/* Sort columns. */
enum
{
	SORT_NAME = 0,
	SORT_SIZE,
	SORT_TYPE,
	SORT_MOD
};

/* UI mode: plain browsing, an inline rename edit, or a delete confirmation. */
enum
{
	MODE_BROWSE = 0,
	MODE_RENAME,
	MODE_CONFIRM_DEL
};

/* One directory entry. */
struct fentry
{
	char name[NAME_MAX_LEN];
	uint64_t size;
	int64_t mtime;
	bool is_dir;
};

/* A clickable breadcrumb segment: its on-screen span and the path it points to. */
struct crumb
{
	int x0, x1;
	char path[512];
};

/* ── state ──────────────────────────────────────────────────────────────────*/
static const char g_mbox[] = "files";
static const char g_views[] = "views";

static ogSurface g_surf;
static ogScalableFont g_font;
static uint32_t g_win_id;
static int32_t g_w = WIN_W, g_h = WIN_H;

static char g_cwd[512] = "/";
static struct fentry g_ent[MAX_ENTRIES];
static int g_nent;
static int g_sel = -1; /* selected row, or -1 */
static int g_top = 0;  /* first visible row (scroll offset) */
static int g_sort = SORT_NAME;
static bool g_sort_desc;

static struct crumb g_crumb[MAX_CRUMBS];
static int g_ncrumb;

static int g_mode = MODE_BROWSE;
static char g_edit[NAME_MAX_LEN]; /* rename edit buffer */
static int g_editlen;

static char g_status[160];

/* Double-click tracking. */
static int g_last_click_row = -1;
static int64_t g_last_click_ms;

/* Toolbar buttons: Back, Fwd, Up, New Folder, Rename, Delete. */
enum
{
	BTN_BACK = 0,
	BTN_FWD,
	BTN_UP,
	BTN_NEWDIR,
	BTN_RENAME,
	BTN_DELETE,
	BTN_COUNT
};
static ogButton g_btn[BTN_COUNT];
static const char *g_btn_label[BTN_COUNT] = {"\x1b", "\x1a", "Up", "New Folder", "Rename", "Delete"};

/* Back/forward history of visited directories. */
#define HIST_MAX 64
static char g_hist[HIST_MAX][512];
static int g_hist_len; /* number of valid entries */
static int g_hist_pos; /* index of the current dir within g_hist */

/* ── helpers ────────────────────────────────────────────────────────────────*/

/**
 * Milliseconds since the epoch, for double-click timing.
 */
static int64_t now_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/**
 * Set the scalable font's foreground from a packed 0xRRGGBB colour.
 */
static void set_fg(uint32_t c)
{
	g_font.SetFGColor((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}

/**
 * Human-readable byte size into @out (e.g. "4.0 KB", "12 B").
 */
static void hsize(uint64_t bytes, char *out, size_t outsz)
{
	static const char unit[] = "BKMGT";
	uint64_t div = 1;
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

/**
 * Lower-case file extension (without the dot) of @name into @out; empty if none.
 */
static void ext_of(const char *name, char *out, size_t outsz)
{
	const char *dot = strrchr(name, '.');
	out[0] = '\0';
	if (!dot || dot == name || dot[1] == '\0')
		return;
	size_t j = 0;
	for (const char *p = dot + 1; *p && j < outsz - 1; p++)
	{
		char c = *p;
		if (c >= 'A' && c <= 'Z')
			c = (char)(c - 'A' + 'a');
		out[j++] = c;
	}
	out[j] = '\0';
}

/**
 * Human-facing type label for an entry ("Folder", "TXT file", "File").
 */
static void type_of(const struct fentry *e, char *out, size_t outsz)
{
	if (e->is_dir)
	{
		snprintf(out, outsz, "Folder");
		return;
	}
	char ext[24];
	ext_of(e->name, ext, sizeof(ext));
	if (ext[0])
	{
		/* Upper-case the extension for display. */
		char up[24];
		size_t j = 0;
		for (; ext[j] && j < sizeof(up) - 1; j++)
			up[j] = (ext[j] >= 'a' && ext[j] <= 'z') ? (char)(ext[j] - 'a' + 'A') : ext[j];
		up[j] = '\0';
		snprintf(out, outsz, "%s file", up);
	}
	else
		snprintf(out, outsz, "File");
}

/**
 * Join directory @dir and entry @name into an absolute path in @out, collapsing
 * the root's trailing slash so "/" + "bin" yields "/bin", not "//bin".
 */
static void join_path(const char *dir, const char *name, char *out, size_t outsz)
{
	if (dir[0] == '/' && dir[1] == '\0')
		snprintf(out, outsz, "/%s", name);
	else
		snprintf(out, outsz, "%s/%s", dir, name);
}

/**
 * Parent directory of @path into @out ("/a/b" → "/a", "/a" → "/", "/" → "/").
 */
static void parent_of(const char *path, char *out, size_t outsz)
{
	snprintf(out, outsz, "%s", path);
	char *slash = strrchr(out, '/');
	if (!slash || slash == out)
	{
		snprintf(out, outsz, "/");
		return;
	}
	*slash = '\0';
}

/* ── data ───────────────────────────────────────────────────────────────────*/

/**
 * Order comparator: directories first, then by the active sort column, honoring
 * the descending flag.  Used by qsort over g_ent.
 */
static int entry_cmp(const void *a, const void *b)
{
	const struct fentry *ea = (const struct fentry *)a;
	const struct fentry *eb = (const struct fentry *)b;
	if (ea->is_dir != eb->is_dir)
		return ea->is_dir ? -1 : 1; /* folders always lead */

	int r = 0;
	switch (g_sort)
	{
		case SORT_SIZE:
			r = (ea->size < eb->size) ? -1 : (ea->size > eb->size) ? 1 : 0;
			break;
		case SORT_MOD:
			r = (ea->mtime < eb->mtime) ? -1 : (ea->mtime > eb->mtime) ? 1 : 0;
			break;
		case SORT_TYPE:
		{
			char ta[24], tb[24];
			ext_of(ea->name, ta, sizeof(ta));
			ext_of(eb->name, tb, sizeof(tb));
			r = strcmp(ta, tb);
			break;
		}
		default:
			break;
	}
	if (r == 0)
		r = strcmp(ea->name, eb->name); /* name is the tiebreak (and SORT_NAME) */
	return g_sort_desc ? -r : r;
}

/**
 * Re-read g_cwd into g_ent: one stat() per entry for size / mtime / type, with
 * "." and ".." dropped (Up and the breadcrumbs handle ascent).  Re-sorts and
 * clears the selection.
 */
static int read_dir(void)
{
	g_nent = 0;
	g_sel = -1;
	g_top = 0;

	DIR *d = opendir(g_cwd);
	if (!d)
	{
		snprintf(g_status, sizeof(g_status), "Cannot open %s", g_cwd);
		return -1;
	}
	struct dirent *de;
	while ((de = readdir(d)) != 0 && g_nent < MAX_ENTRIES)
	{
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
			continue;
		struct fentry *e = &g_ent[g_nent];
		snprintf(e->name, sizeof(e->name), "%s", de->d_name);

		char full[768];
		join_path(g_cwd, de->d_name, full, sizeof(full));
		struct stat st;
		if (stat(full, &st) == 0)
		{
			e->is_dir = S_ISDIR(st.st_mode);
			e->size = (uint64_t)st.st_size;
			e->mtime = (int64_t)st.st_mtime;
		}
		else
		{
			/* Fall back to readdir's type hint if stat fails. */
			e->is_dir = (de->d_type == DT_DIR);
			e->size = 0;
			e->mtime = 0;
		}
		g_nent++;
	}
	closedir(d);
	qsort(g_ent, g_nent, sizeof(g_ent[0]), entry_cmp);
	return 0;
}

/**
 * Push @path onto the back/forward history, truncating any forward entries (a
 * fresh navigation discards the redo stack, exactly like a web browser).
 */
static void hist_push(const char *path)
{
	if (g_hist_len > 0 && strcmp(g_hist[g_hist_pos], path) == 0)
		return; /* re-entering the same dir: no new history node */
	g_hist_pos = (g_hist_len == 0) ? 0 : g_hist_pos + 1;
	if (g_hist_pos >= HIST_MAX)
	{
		/* Slide the window down one to make room. */
		for (int i = 1; i < HIST_MAX; i++)
			memcpy(g_hist[i - 1], g_hist[i], sizeof(g_hist[0]));
		g_hist_pos = HIST_MAX - 1;
	}
	snprintf(g_hist[g_hist_pos], sizeof(g_hist[0]), "%s", path);
	g_hist_len = g_hist_pos + 1;
}

/**
 * Switch to @path and reload it.  When @record, the move is pushed onto history
 * (normal navigation); Back/Forward pass false so they don't rewrite it.
 */
static void navigate(const char *path, bool record)
{
	if (path != g_cwd) /* guard against a self-overlapping copy (UB in snprintf) */
		snprintf(g_cwd, sizeof(g_cwd), "%s", path);
	if (record)
		hist_push(g_cwd);
	if (read_dir() == 0)
		snprintf(g_status, sizeof(g_status), "%d item%s", g_nent, g_nent == 1 ? "" : "s");
	/* On failure read_dir leaves an explanatory message in g_status. */
}

/* ── actions ────────────────────────────────────────────────────────────────*/

/**
 * Open entry @i: enter it if a folder, else look up its extension handler in the
 * registry (/files/assoc/<ext>/exec) and execve it on the file.  A missing
 * handler is reported in the status bar rather than failing silently.
 */
static void open_entry(int i)
{
	if (i < 0 || i >= g_nent)
		return;
	struct fentry *e = &g_ent[i];
	char full[768];
	join_path(g_cwd, e->name, full, sizeof(full));

	if (e->is_dir)
	{
		navigate(full, true);
		return;
	}

	char ext[24], key[64], app[256];
	ext_of(e->name, ext, sizeof(ext));
	if (ext[0])
	{
		snprintf(key, sizeof(key), "/files/assoc/%s/exec", ext);
		if (ubistry_get_str(key, app, sizeof(app)) == 0 && app[0])
		{
			if (fork() == 0)
			{
				char *argv[3] = {app, full, 0};
				execve(app, argv, environ);
				_exit(127);
			}
			snprintf(g_status, sizeof(g_status), "Opening %s", e->name);
			return;
		}
	}
	snprintf(g_status, sizeof(g_status), "No handler for %s", ext[0] ? ext : e->name);
}

/**
 * Create a new folder in g_cwd, choosing the first free "New Folder", "New
 * Folder 2", … name.  Reloads and selects the result so it can be renamed.
 */
static void new_folder(void)
{
	char name[NAME_MAX_LEN], full[768];
	for (int n = 1; n < 1000; n++)
	{
		if (n == 1)
			snprintf(name, sizeof(name), "New Folder");
		else
			snprintf(name, sizeof(name), "New Folder %d", n);
		join_path(g_cwd, name, full, sizeof(full));
		struct stat st;
		if (stat(full, &st) != 0)
			break; /* free name */
	}
	if (mkdir(full, 0755) != 0)
	{
		snprintf(g_status, sizeof(g_status), "Could not create folder");
		return;
	}
	read_dir();
	for (int i = 0; i < g_nent; i++)
		if (strcmp(g_ent[i].name, name) == 0)
		{
			g_sel = i;
			break;
		}
	snprintf(g_status, sizeof(g_status), "Created %s", name);
}

/**
 * Delete the selected entry (rmdir for a folder, unlink for a file).  rmdir only
 * removes empty folders in P0 — recursive delete waits for the Trash phase.
 */
static void do_delete(void)
{
	if (g_sel < 0 || g_sel >= g_nent)
		return;
	struct fentry *e = &g_ent[g_sel];
	char full[768];
	join_path(g_cwd, e->name, full, sizeof(full));
	int rc = e->is_dir ? rmdir(full) : unlink(full);
	if (rc != 0)
		snprintf(
		    g_status, sizeof(g_status), "Could not delete %s%s", e->name, e->is_dir ? " (not empty?)" : "");
	else
		snprintf(g_status, sizeof(g_status), "Deleted %s", e->name);
	read_dir();
}

/**
 * Commit the inline rename: rename(old → g_edit) within g_cwd, then reload and
 * re-select the entry under its new name.
 */
static void commit_rename(void)
{
	if (g_sel < 0 || g_sel >= g_nent || g_editlen == 0)
	{
		g_mode = MODE_BROWSE;
		return;
	}
	char oldp[768], newp[768];
	join_path(g_cwd, g_ent[g_sel].name, oldp, sizeof(oldp));
	join_path(g_cwd, g_edit, newp, sizeof(newp));
	if (rename(oldp, newp) != 0)
		snprintf(g_status, sizeof(g_status), "Could not rename");
	else
		snprintf(g_status, sizeof(g_status), "Renamed to %s", g_edit);
	g_mode = MODE_BROWSE;
	read_dir();
	for (int i = 0; i < g_nent; i++)
		if (strcmp(g_ent[i].name, g_edit) == 0)
		{
			g_sel = i;
			break;
		}
}

/**
 * Begin an inline rename of the selected entry, seeding the edit buffer with its
 * current name.
 */
static void start_rename(void)
{
	if (g_sel < 0 || g_sel >= g_nent)
		return;
	snprintf(g_edit, sizeof(g_edit), "%s", g_ent[g_sel].name);
	g_editlen = (int)strlen(g_edit);
	g_mode = MODE_RENAME;
}

/* ── layout queries ─────────────────────────────────────────────────────────*/

/** Y of the first list row (below toolbar + address bar + header). */
static int list_top(void)
{
	return TOOLBAR_H + ADDR_H + HEAD_H;
}

/** Number of fully-visible list rows for the current window height. */
static int visible_rows(void)
{
	int h = g_h - STATUS_H - list_top();
	return h > 0 ? h / ROW_H : 0;
}

/** Right-edge X of the size column (size is right-aligned here). */
static int col_size_r(void)
{
	return g_w - PAD - 150 - 90;
}
/** Left-edge X of the type column. */
static int col_type_x(void)
{
	return g_w - PAD - 150 - 80;
}
/** Left-edge X of the modified column. */
static int col_mod_x(void)
{
	return g_w - PAD - 150;
}

/* ── drawing ────────────────────────────────────────────────────────────────*/

/**
 * Draw a 16px folder or document glyph at (x,y) with objGFX primitives (no icon
 * assets yet — a small set comes in a later phase).
 */
static void draw_icon(int x, int y, bool is_dir)
{
	if (is_dir)
	{
		g_surf.ogFillRect(x, y + 4, x + 15, y + 13, COL_FOLDER);
		g_surf.ogFillRect(x, y + 2, x + 6, y + 5, COL_FOLDER); /* tab */
	}
	else
	{
		g_surf.ogFillRect(x + 1, y + 1, x + 12, y + 14, COL_FILE);
		g_surf.ogFillRect(x + 9, y + 1, x + 12, y + 4, COL_FILE_FOLD); /* folded corner */
	}
}

/**
 * Draw the toolbar and lay out / hit-enable its buttons (Back/Fwd gated on
 * history position; Rename/Delete gated on a selection).
 */
static void draw_toolbar(void)
{
	g_surf.ogFillRect(0, 0, g_w - 1, TOOLBAR_H - 1, COL_TOOLBAR);
	g_surf.ogHLine(0, g_w - 1, TOOLBAR_H - 1, COL_DIVIDER);

	bool has_sel = (g_sel >= 0);
	bool can_back = (g_hist_pos > 0);
	bool can_fwd = (g_hist_pos + 1 < g_hist_len);

	int bx = 8;
	for (int i = 0; i < BTN_COUNT; i++)
	{
		int bw = (int)g_font.TextWidth(g_btn_label[i]) + 22;
		if (i == BTN_BACK || i == BTN_FWD || i == BTN_UP)
			bw = 34;
		g_btn[i].x = bx;
		g_btn[i].y = 7;
		g_btn[i].w = bw;
		g_btn[i].h = TOOLBAR_H - 14;
		g_btn[i].label = g_btn_label[i];
		switch (i)
		{
			case BTN_BACK:
				g_btn[i].enabled = can_back;
				break;
			case BTN_FWD:
				g_btn[i].enabled = can_fwd;
				break;
			case BTN_UP:
				g_btn[i].enabled = !(g_cwd[0] == '/' && g_cwd[1] == '\0');
				break;
			case BTN_RENAME:
			case BTN_DELETE:
				g_btn[i].enabled = has_sel;
				break;
			default:
				g_btn[i].enabled = true;
				break;
		}
		g_btn[i].Draw(g_surf, g_font);
		bx += bw + 6;
		if (i == BTN_UP)
			bx += 10; /* small gap before the action group */
	}
}

/**
 * Draw the breadcrumb address bar, recording each segment's span + target path
 * in g_crumb for click navigation.
 */
static void draw_address(void)
{
	int ay = TOOLBAR_H;
	g_surf.ogFillRect(0, ay, g_w - 1, ay + ADDR_H - 1, COL_ADDR);
	g_surf.ogHLine(0, g_w - 1, ay + ADDR_H - 1, COL_DIVIDER);

	g_ncrumb = 0;
	int x = PAD;
	int ty = ay + (ADDR_H - (int)g_font.GetHeight()) / 2;

	/* Root crumb. */
	set_fg(COL_CRUMB);
	g_font.PutString(g_surf, x, ty, "/");
	g_crumb[g_ncrumb].x0 = x;
	g_crumb[g_ncrumb].x1 = x + (int)g_font.TextWidth("/");
	snprintf(g_crumb[g_ncrumb].path, sizeof(g_crumb[0].path), "/");
	x = g_crumb[g_ncrumb].x1 + 4;
	g_ncrumb++;

	/* Walk the path components, accumulating the prefix for each crumb. */
	char acc[512] = "";
	const char *p = g_cwd;
	while (*p == '/')
		p++;
	char comp[NAME_MAX_LEN];
	while (*p && g_ncrumb < MAX_CRUMBS)
	{
		int j = 0;
		while (*p && *p != '/' && j < (int)sizeof(comp) - 1)
			comp[j++] = *p++;
		comp[j] = '\0';
		while (*p == '/')
			p++;
		if (j == 0)
			break;

		size_t al = strlen(acc);
		snprintf(acc + al, sizeof(acc) - al, "/%s", comp);

		set_fg(COL_TEXT_DIM);
		g_font.PutString(g_surf, x, ty, ">");
		x += (int)g_font.TextWidth(">") + 4;

		set_fg(COL_CRUMB);
		g_font.PutString(g_surf, x, ty, comp);
		g_crumb[g_ncrumb].x0 = x;
		g_crumb[g_ncrumb].x1 = x + (int)g_font.TextWidth(comp);
		snprintf(g_crumb[g_ncrumb].path, sizeof(g_crumb[0].path), "%s", acc);
		x = g_crumb[g_ncrumb].x1 + 4;
		g_ncrumb++;
	}
}

/**
 * Draw one sortable column header label, with a ▲/▼ marker when it is the active
 * sort key.
 */
static void draw_head_label(int x, const char *label, int col)
{
	set_fg(col == g_sort ? COL_TEXT : COL_TEXT_DIM);
	char buf[48];
	if (col == g_sort)
		snprintf(buf, sizeof(buf), "%s %s", label, g_sort_desc ? "\x19" : "\x18");
	else
		snprintf(buf, sizeof(buf), "%s", label);
	g_font.PutString(g_surf, x, TOOLBAR_H + ADDR_H + (HEAD_H - (int)g_font.GetHeight()) / 2, buf);
}

/**
 * Draw the column-header row (Name / Size / Type / Modified).
 */
static void draw_header(void)
{
	int hy = TOOLBAR_H + ADDR_H;
	g_surf.ogFillRect(0, hy, g_w - 1, hy + HEAD_H - 1, COL_HEAD);
	g_surf.ogHLine(0, g_w - 1, hy + HEAD_H - 1, COL_DIVIDER);
	draw_head_label(PAD + ICON_W + 6, "Name", SORT_NAME);
	draw_head_label(col_size_r() - (int)g_font.TextWidth("Size") - 14, "Size", SORT_SIZE);
	draw_head_label(col_type_x(), "Type", SORT_TYPE);
	draw_head_label(col_mod_x(), "Modified", SORT_MOD);
}

/**
 * Draw the visible slice of the file list, including row striping, the selection
 * highlight, and the inline rename editor when active.
 */
static void draw_list(void)
{
	int top = list_top();
	int bottom = g_h - STATUS_H;
	g_surf.ogFillRect(0, top, g_w - 1, bottom - 1, COL_WIN);

	int rows = visible_rows();
	for (int r = 0; r < rows; r++)
	{
		int i = g_top + r;
		if (i >= g_nent)
			break;
		struct fentry *e = &g_ent[i];
		int y = top + r * ROW_H;
		bool sel = (i == g_sel);

		if (sel)
			g_surf.ogFillRect(0, y, g_w - 1, y + ROW_H - 1, COL_SEL);
		else if (i & 1)
			g_surf.ogFillRect(0, y, g_w - 1, y + ROW_H - 1, COL_ROW_ALT);

		draw_icon(PAD, y + (ROW_H - 16) / 2, e->is_dir);

		int ty = y + (ROW_H - (int)g_font.GetHeight()) / 2;
		uint32_t fg = sel ? COL_SEL_TEXT : COL_TEXT;
		uint32_t dim = sel ? COL_SEL_TEXT : COL_TEXT_DIM;

		/* Name (inline editor for the row being renamed). */
		if (g_mode == MODE_RENAME && sel)
		{
			int ex = PAD + ICON_W + 6;
			g_surf.ogFillRect(ex - 2, y + 2, col_size_r() - 16, y + ROW_H - 3, COL_ADDR);
			g_surf.ogRoundRect(ex - 2, y + 2, col_size_r() - 16, y + ROW_H - 3, 3, COL_SEL);
			set_fg(COL_TEXT);
			char shown[NAME_MAX_LEN + 2];
			snprintf(shown, sizeof(shown), "%s|", g_edit);
			g_font.PutString(g_surf, ex, ty, shown);
		}
		else
		{
			set_fg(fg);
			g_font.PutString(g_surf, PAD + ICON_W + 6, ty, e->name);
		}

		/* Size (right-aligned), folders show "--". */
		char sz[24];
		if (e->is_dir)
			snprintf(sz, sizeof(sz), "--");
		else
			hsize(e->size, sz, sizeof(sz));
		set_fg(dim);
		g_font.PutString(g_surf, col_size_r() - (int)g_font.TextWidth(sz) - 14, ty, sz);

		/* Type. */
		char ty_s[40];
		type_of(e, ty_s, sizeof(ty_s));
		g_font.PutString(g_surf, col_type_x(), ty, ty_s);

		/* Modified. */
		if (e->mtime > 0)
		{
			time_t t = (time_t)e->mtime;
			struct tm *tmv = localtime(&t);
			if (tmv)
			{
				char mod[40];
				snprintf(mod,
				         sizeof(mod),
				         "%04d-%02d-%02d %02d:%02d",
				         tmv->tm_year + 1900,
				         tmv->tm_mon + 1,
				         tmv->tm_mday,
				         tmv->tm_hour,
				         tmv->tm_min);
				g_font.PutString(g_surf, col_mod_x(), ty, mod);
			}
		}
	}
}

/**
 * Draw the status bar: item count plus the selected entry's name/size, or a
 * transient action message.
 */
static void draw_status(void)
{
	int sy = g_h - STATUS_H;
	g_surf.ogFillRect(0, sy, g_w - 1, g_h - 1, COL_TOOLBAR);
	g_surf.ogHLine(0, g_w - 1, sy, COL_DIVIDER);
	set_fg(COL_TEXT_DIM);
	int ty = sy + (STATUS_H - (int)g_font.GetHeight()) / 2;
	g_font.PutString(g_surf, PAD, ty, g_status);

	if (g_sel >= 0 && g_sel < g_nent)
	{
		char right[80];
		if (g_ent[g_sel].is_dir)
			snprintf(right, sizeof(right), "%s", g_ent[g_sel].name);
		else
		{
			char sz[24];
			hsize(g_ent[g_sel].size, sz, sizeof(sz));
			snprintf(right, sizeof(right), "%s  -  %s", g_ent[g_sel].name, sz);
		}
		g_font.PutString(g_surf, g_w - PAD - (int)g_font.TextWidth(right), ty, right);
	}
}

/**
 * Draw the centered "Delete?" confirmation dialog over the dimmed window.
 */
static void draw_confirm(void)
{
	int dw = 320, dh = 130;
	int dx = (g_w - dw) / 2, dy = (g_h - dh) / 2;
	g_surf.ogFillRoundRect(dx, dy, dx + dw, dy + dh, 8, COL_WIN);
	g_surf.ogRoundRect(dx, dy, dx + dw, dy + dh, 8, COL_DIVIDER);

	set_fg(COL_TEXT);
	char msg[128];
	snprintf(msg, sizeof(msg), "Delete \"%s\"?", g_sel >= 0 ? g_ent[g_sel].name : "");
	g_font.PutString(g_surf, dx + 20, dy + 24, msg);
	set_fg(COL_TEXT_DIM);
	g_font.PutString(g_surf, dx + 20, dy + 24 + (int)g_font.GetHeight() + 6, "This cannot be undone.");

	/* Reuse the Delete/Rename slots as the dialog buttons for hit-testing. */
	g_btn[BTN_DELETE].x = dx + dw - 100;
	g_btn[BTN_DELETE].y = dy + dh - 40;
	g_btn[BTN_DELETE].w = 84;
	g_btn[BTN_DELETE].h = 28;
	g_btn[BTN_DELETE].label = "Delete";
	g_btn[BTN_DELETE].enabled = true;
	g_btn[BTN_DELETE].Draw(g_surf, g_font);

	g_btn[BTN_RENAME].x = dx + dw - 196;
	g_btn[BTN_RENAME].y = dy + dh - 40;
	g_btn[BTN_RENAME].w = 84;
	g_btn[BTN_RENAME].h = 28;
	g_btn[BTN_RENAME].label = "Cancel";
	g_btn[BTN_RENAME].enabled = true;
	g_btn[BTN_RENAME].Draw(g_surf, g_font);
}

/**
 * Post a full-window DISPLAY_FLIP so the compositor recomposites our buffer.
 */
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

/**
 * Repaint the whole window and flip it to screen.
 */
static void render(void)
{
	g_surf.ogClear(COL_WIN);
	draw_list();
	draw_header();
	draw_address();
	draw_toolbar();
	draw_status();
	if (g_mode == MODE_CONFIRM_DEL)
		draw_confirm();
	flip();
}

/* ── input ──────────────────────────────────────────────────────────────────*/

/**
 * Keep the selected row within the scrolled viewport, adjusting g_top as needed.
 */
static void ensure_visible(void)
{
	if (g_sel < 0)
		return;
	int rows = visible_rows();
	if (g_sel < g_top)
		g_top = g_sel;
	else if (g_sel >= g_top + rows)
		g_top = g_sel - rows + 1;
	if (g_top < 0)
		g_top = 0;
}

/**
 * Handle a left-button press at (x,y): toolbar buttons, breadcrumb segments,
 * column headers, and row select / double-click-to-open.
 */
static void on_click(int x, int y)
{
	/* Modal: only the dialog buttons are live. */
	if (g_mode == MODE_CONFIRM_DEL)
	{
		if (g_btn[BTN_DELETE].Hit(x, y))
		{
			g_mode = MODE_BROWSE;
			do_delete();
		}
		else if (g_btn[BTN_RENAME].Hit(x, y))
			g_mode = MODE_BROWSE;
		render();
		return;
	}
	if (g_mode == MODE_RENAME)
	{
		commit_rename(); /* clicking elsewhere commits, like Explorer */
		render();
		return;
	}

	/* Toolbar. */
	if (y < TOOLBAR_H)
	{
		if (g_btn[BTN_BACK].Hit(x, y) && g_hist_pos > 0)
			navigate(g_hist[--g_hist_pos], false);
		else if (g_btn[BTN_FWD].Hit(x, y) && g_hist_pos + 1 < g_hist_len)
			navigate(g_hist[++g_hist_pos], false);
		else if (g_btn[BTN_UP].Hit(x, y))
		{
			char up[512];
			parent_of(g_cwd, up, sizeof(up));
			navigate(up, true);
		}
		else if (g_btn[BTN_NEWDIR].Hit(x, y))
			new_folder();
		else if (g_btn[BTN_RENAME].Hit(x, y))
			start_rename();
		else if (g_btn[BTN_DELETE].Hit(x, y) && g_sel >= 0)
			g_mode = MODE_CONFIRM_DEL;
		render();
		return;
	}

	/* Breadcrumb bar. */
	if (y >= TOOLBAR_H && y < TOOLBAR_H + ADDR_H)
	{
		for (int i = 0; i < g_ncrumb; i++)
			if (x >= g_crumb[i].x0 - 2 && x <= g_crumb[i].x1 + 2)
			{
				navigate(g_crumb[i].path, true);
				render();
				return;
			}
		return;
	}

	/* Column header → sort. */
	if (y >= TOOLBAR_H + ADDR_H && y < list_top())
	{
		int col;
		if (x >= col_mod_x())
			col = SORT_MOD;
		else if (x >= col_type_x())
			col = SORT_TYPE;
		else if (x >= col_size_r() - 90)
			col = SORT_SIZE;
		else
			col = SORT_NAME;
		if (col == g_sort)
			g_sort_desc = !g_sort_desc;
		else
		{
			g_sort = col;
			g_sort_desc = false;
		}
		qsort(g_ent, g_nent, sizeof(g_ent[0]), entry_cmp);
		render();
		return;
	}

	/* List row. */
	if (y >= list_top() && y < g_h - STATUS_H)
	{
		int r = (y - list_top()) / ROW_H;
		int i = g_top + r;
		if (i < 0 || i >= g_nent)
		{
			g_sel = -1;
			render();
			return;
		}
		int64_t t = now_ms();
		bool dbl = (i == g_last_click_row && (t - g_last_click_ms) < 400);
		g_sel = i;
		g_last_click_row = i;
		g_last_click_ms = t;
		if (dbl)
			open_entry(i);
		render();
	}
}

/**
 * Handle a key press.  Routes to the rename editor or delete dialog when modal;
 * otherwise drives selection, navigation, type-ahead, and the action shortcuts.
 */
static void on_key(uint32_t kc)
{
	/* Delete confirmation. */
	if (g_mode == MODE_CONFIRM_DEL)
	{
		if (kc == '\r' || kc == '\n' || kc == 'y' || kc == 'Y')
		{
			g_mode = MODE_BROWSE;
			do_delete();
		}
		else if (kc == KEY_ESC || kc == 'n' || kc == 'N')
			g_mode = MODE_BROWSE;
		render();
		return;
	}

	/* Inline rename editor. */
	if (g_mode == MODE_RENAME)
	{
		if (kc == '\r' || kc == '\n')
			commit_rename();
		else if (kc == KEY_ESC)
			g_mode = MODE_BROWSE;
		else if (kc == '\b' || kc == 0x7F)
		{
			if (g_editlen > 0)
				g_edit[--g_editlen] = '\0';
		}
		else if (kc >= 0x20 && kc < 0x7F && g_editlen < NAME_MAX_LEN - 1)
		{
			g_edit[g_editlen++] = (char)kc;
			g_edit[g_editlen] = '\0';
		}
		render();
		return;
	}

	/* Browsing. */
	switch (kc)
	{
		case KEY_ESC:
		{
			mpi_message_t rel;
			memset(&rel, 0, sizeof(rel));
			rel.header = DISPLAY_RELEASE;
			((struct display_release *)rel.data)->window_id = g_win_id;
			mpi_postMessage((char *)g_views, DISPLAY_RELEASE, &rel);
			mpi_destroyMbox((char *)g_mbox);
			exit(0);
		}
		case KEY_UP:
			if (g_sel > 0)
				g_sel--;
			else if (g_sel < 0 && g_nent > 0)
				g_sel = 0;
			ensure_visible();
			break;
		case KEY_DOWN:
			if (g_sel < g_nent - 1)
				g_sel++;
			ensure_visible();
			break;
		case KEY_PGUP:
			g_sel -= visible_rows();
			if (g_sel < 0)
				g_sel = 0;
			ensure_visible();
			break;
		case KEY_PGDN:
			g_sel += visible_rows();
			if (g_sel >= g_nent)
				g_sel = g_nent - 1;
			ensure_visible();
			break;
		case KEY_HOME:
			g_sel = g_nent ? 0 : -1;
			ensure_visible();
			break;
		case KEY_END:
			g_sel = g_nent - 1;
			ensure_visible();
			break;
		case '\r':
		case '\n':
			if (g_sel >= 0)
				open_entry(g_sel);
			break;
		case '\b':
		case 0x7F: /* Backspace → up a level */
		{
			char up[512];
			parent_of(g_cwd, up, sizeof(up));
			navigate(up, true);
			break;
		}
		case KEY_F2:
			start_rename();
			break;
		case KEY_DEL:
			if (g_sel >= 0)
				g_mode = MODE_CONFIRM_DEL;
			break;
		default:
			/* Type-ahead: jump to the next entry whose name starts with kc. */
			if (kc >= 0x20 && kc < 0x7F)
			{
				char c = (char)kc;
				if (c >= 'A' && c <= 'Z')
					c = (char)(c - 'A' + 'a');
				for (int n = 1; n <= g_nent; n++)
				{
					int i = (g_sel + n) % (g_nent ? g_nent : 1);
					char f = g_ent[i].name[0];
					if (f >= 'A' && f <= 'Z')
						f = (char)(f - 'A' + 'a');
					if (f == c)
					{
						g_sel = i;
						ensure_visible();
						break;
					}
				}
			}
			break;
	}
	render();
}

/* ── main ───────────────────────────────────────────────────────────────────*/

/**
 * Claim a window from views, attach the shared buffer, and run the MPI event
 * loop (blocking on the mailbox so an idle Files window stays off the CPU).
 */
int main(int argc, char **argv)
{
	mpi_createMbox((char *)g_mbox);

	/* Pick a starting directory: an absolute path argument, else the session
	 * HOME, else "/".  Fall back to "/" if the chosen dir cannot be opened so we
	 * never launch into an empty window.  Kept in a local buffer (navigate copies
	 * it into g_cwd — passing g_cwd to itself would be an overlapping copy). */
	char start[512];
	if (argc > 1 && argv[1][0] == '/')
		snprintf(start, sizeof(start), "%s", argv[1]);
	else
	{
		const char *home = getenv("HOME");
		snprintf(start, sizeof(start), "%s", (home && home[0]) ? home : "/");
	}
	DIR *probe = opendir(start);
	if (probe)
		closedir(probe);
	else
		snprintf(start, sizeof(start), "/");

	mpi_message_t msg;
	memset(&msg, 0, sizeof(msg));
	msg.header = DISPLAY_CLAIM;
	struct display_claim_req *req = (struct display_claim_req *)msg.data;
	req->x = 80;
	req->y = 60;
	req->w = WIN_W;
	req->h = WIN_H;
	req->sender_pid = getpid();
	strncpy(req->title, "Files", sizeof(req->title) - 1);
	strncpy(req->reply, g_mbox, sizeof(req->reply) - 1);
	req->min_w = 520;
	req->min_h = 320;
	req->max_w = 1200;
	req->max_h = 900;
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

	navigate(start, true);
	render();

	for (;;)
	{
		mpi_message_t ev;
		if (mpi_waitMessage((char *)g_mbox, &ev, 0) != 0) /* block until a message */
			continue;
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
				ensure_visible();
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
				if (k->pressed)
					on_key(k->keycode);
				break;
			}
			default:
				break;
		}
	}
	return 0;
}
