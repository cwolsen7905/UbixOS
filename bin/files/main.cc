/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Files — the uBixOS graphical file manager (P0 MVP).
 *
 * An Explorer-leaning, best-of-all-worlds file browser: a navigation toolbar
 * (back / forward / up), a breadcrumb address bar that turns into an editable
 * path field when clicked, a left "Places" sidebar (Home, Filesystem, top-level
 * folders), a sortable Details list (Name / Size / Type / Modified) or a large-
 * icon grid (toggled in the toolbar) with a drag/page/wheel scrollbar, a filter
 * box, and a status bar showing item count and free space.  Double-click a folder to
 * enter it, a file to open it in the app the ubistry registry maps from its
 * extension.  File operations — New Folder, Open, Rename, Delete — live in the
 * right-click context menu (Explorer-style), not on the toolbar.  This is a
 * standard views + objGFX client (one process, a shared-memory window buffer,
 * MPI for window control), built the same way as bin/diskutil and bin/activity.
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
#include <api/ubfs_pool.h>

	extern char **environ; /* inherited session env, forwarded to opened apps */
}
#include <objgfx/objgfx.h>
#include <objgfx/ogScalableFont.h>
#include <objgfx/ogButton.h>

#define RGB(r, g, b) ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))

#define WIN_W 820
#define WIN_H 500
#define TOOLBAR_H 44
#define ADDR_H 32     /* breadcrumb address bar */
#define SIDEBAR_W 176 /* left navigation sidebar */
#define HEAD_H 26     /* column-header row */
#define ROW_H 24      /* one list entry */
#define STATUS_H 26
#define SCROLLBAR_W 12
#define SCROLL_THUMB_MIN 28
#define PAD 10
#define ICON_W 18
#define FONT_PATH "/var/fonts/DejaVuSans.ttf"

#define MAX_ENTRIES 1024
#define MAX_CRUMBS 64
#define MAX_PLACES 24
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

/* List presentation: a Details table or a grid of large icons. */
enum
{
	VIEW_DETAILS = 0,
	VIEW_ICONS
};

#define CELL_W 120
#define CELL_H 92
#define BIG_ICON 48

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
static int g_view = VIEW_DETAILS;

static struct crumb g_crumb[MAX_CRUMBS];
static int g_ncrumb;

/* Sidebar "Places": pinned/quick-access destinations. */
struct place
{
	char label[64];
	char path[512];
	bool is_home;
};
static struct place g_places[MAX_PLACES];
static int g_nplaces;

static int g_mode = MODE_BROWSE;
static char g_edit[NAME_MAX_LEN]; /* rename edit buffer */
static int g_editlen;

/* Editable address bar: click the path bar to type a destination. */
static bool g_addr_edit;
static char g_addr_buf[512];
static int g_addr_len;

/* Toolbar filter box: narrows the list to names containing the substring. */
static bool g_filter_edit;
static char g_filter[64];
static int g_filter_len;
static int g_filter_x, g_filter_w; /* filter box rect (set during draw_toolbar) */

static char g_status[160];
static char g_free_str[48]; /* "1.2 GB free" for the current filesystem, or "" */

/* Double-click tracking. */
static int g_last_click_row = -1;
static int64_t g_last_click_ms;

/* Scrollbar drag state. */
static uint8_t g_prev_buttons;
static bool g_scroll_drag;
static int g_scroll_drag_off; /* cursor offset within the thumb at grab time */

/* Toolbar is navigation-only: Back, Forward, Up.  File operations (New Folder,
 * Open, Rename, Delete) live in the right-click context menu, Explorer-style. */
enum
{
	BTN_BACK = 0,
	BTN_FWD,
	BTN_UP,
	BTN_COUNT
};
static ogButton g_btn[BTN_COUNT];
static const char *g_btn_label[BTN_COUNT] = {"<", ">", "Up"};

/* Confirmation-dialog buttons (own objects so they don't alias the toolbar). */
static ogButton g_dlg_ok;
static ogButton g_dlg_cancel;

/* View-mode toggle buttons (Details / Icons), laid out in draw_toolbar. */
static ogButton g_vbtn[2];

/* Right-click context menu. */
enum
{
	ACT_OPEN = 0,
	ACT_NEWFOLDER,
	ACT_RENAME,
	ACT_DELETE,
	ACT_REFRESH
};
struct ctx_item
{
	const char *label;
	int action;
	bool separator_after;
};
static bool g_ctx_open;
static int g_ctx_x, g_ctx_y, g_ctx_w, g_ctx_h;
static struct ctx_item g_ctx_items[8];
static int g_ctx_n;
static int g_ctx_target; /* row the menu acts on, or -1 for the folder background */

#define CTX_ROW_H 24
#define CTX_W 168

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
 * True if @name passes the active filter — a case-insensitive substring match,
 * or always true when the filter is empty.
 */
static bool name_matches(const char *name)
{
	if (!g_filter[0])
		return true;
	size_t fl = strlen(g_filter);
	for (const char *p = name; *p; p++)
	{
		size_t k = 0;
		while (k < fl && p[k])
		{
			char a = p[k], b = g_filter[k];
			if (a >= 'A' && a <= 'Z')
				a = (char)(a - 'A' + 'a');
			if (b >= 'A' && b <= 'Z')
				b = (char)(b - 'A' + 'a');
			if (a != b)
				break;
			k++;
		}
		if (k == fl)
			return true;
	}
	return false;
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
		if (!name_matches(de->d_name))
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

static void update_free_space(void);

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
	update_free_space();
}

/**
 * Refresh g_free_str with the free space of the filesystem holding g_cwd.  Picks
 * the mounted pool whose mountpoint is the longest prefix of the current path
 * (statfs is stubbed in the kernel, so query the real pool capacity instead).
 */
static void update_free_space(void)
{
	g_free_str[0] = '\0';
	struct ubix_pool_info pools[8];
	int n = ubix_pool_query(pools, 8);
	if (n <= 0)
		return;

	int best = -1;
	size_t best_len = 0;
	for (int i = 0; i < n; i++)
	{
		const char *mp = pools[i].mountpoint;
		size_t l = strlen(mp);
		if (l == 0 || strncmp(g_cwd, mp, l) != 0)
			continue;
		/* The match must end on a path boundary (so "/po" doesn't match "/pool"). */
		if (mp[l - 1] != '/' && g_cwd[l] != '/' && g_cwd[l] != '\0')
			continue;
		if (l >= best_len)
		{
			best_len = l;
			best = i;
		}
	}
	if (best < 0)
		best = 0; /* fall back to the first (root) pool */

	uint64_t freeb = pools[best].free_blocks * (uint64_t)pools[best].block_size;
	char sz[24];
	hsize(freeb, sz, sizeof(sz));
	snprintf(g_free_str, sizeof(g_free_str), "%s free", sz);
}

/**
 * Populate the sidebar "Places": Home (if it exists and is not root), the
 * filesystem root, then each top-level directory of "/".  Built once at startup;
 * pinning user folders here is a later phase.
 */
static void build_places(void)
{
	g_nplaces = 0;

	const char *home = getenv("HOME");
	if (home && home[0] && strcmp(home, "/") != 0)
	{
		DIR *h = opendir(home);
		if (h)
		{
			closedir(h);
			snprintf(g_places[g_nplaces].label, sizeof(g_places[0].label), "Home");
			snprintf(g_places[g_nplaces].path, sizeof(g_places[0].path), "%s", home);
			g_places[g_nplaces].is_home = true;
			g_nplaces++;
		}
	}

	snprintf(g_places[g_nplaces].label, sizeof(g_places[0].label), "Filesystem");
	snprintf(g_places[g_nplaces].path, sizeof(g_places[0].path), "/");
	g_places[g_nplaces].is_home = false;
	g_nplaces++;

	/* Top-level directories of "/" as quick jumps. */
	DIR *d = opendir("/");
	if (d)
	{
		struct dirent *de;
		while ((de = readdir(d)) != 0 && g_nplaces < MAX_PLACES)
		{
			if (de->d_name[0] == '.')
				continue;
			char full[600];
			snprintf(full, sizeof(full), "/%s", de->d_name);
			struct stat st;
			if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode))
				continue;
			snprintf(g_places[g_nplaces].label, sizeof(g_places[0].label), "%s", de->d_name);
			snprintf(g_places[g_nplaces].path, sizeof(g_places[0].path), "%s", full);
			g_places[g_nplaces].is_home = false;
			g_nplaces++;
		}
		closedir(d);
	}
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

/**
 * Enter address-bar edit mode, seeding the field with the current path.
 */
static void addr_begin_edit(void)
{
	snprintf(g_addr_buf, sizeof(g_addr_buf), "%s", g_cwd);
	g_addr_len = (int)strlen(g_addr_buf);
	g_addr_edit = true;
}

/**
 * Commit the typed path: trim a trailing slash, then navigate there if it opens
 * as a directory, else report the failure and stay put.  Always leaves edit mode.
 */
static void addr_commit(void)
{
	g_addr_edit = false;

	/* Collapse runs of '/' into one (so "//var", "/var//log" normalize cleanly). */
	char path[512];
	size_t w = 0;
	for (const char *r = g_addr_buf; *r && w < sizeof(path) - 1; r++)
	{
		if (*r == '/' && w > 0 && path[w - 1] == '/')
			continue;
		path[w++] = *r;
	}
	path[w] = '\0';
	if (path[0] != '/') /* a relative entry: anchor it at "/" */
	{
		char tmp[512];
		snprintf(tmp, sizeof(tmp), "/%s", path);
		snprintf(path, sizeof(path), "%s", tmp);
		w = strlen(path);
	}
	while (w > 1 && path[w - 1] == '/') /* drop trailing slashes (keep root "/") */
		path[--w] = '\0';
	if (path[0] == '\0')
		snprintf(path, sizeof(path), "/");

	DIR *d = opendir(path);
	if (d)
	{
		closedir(d);
		navigate(path, true);
	}
	else
		snprintf(g_status, sizeof(g_status), "No such directory: %s", path);
}

/* ── layout queries ─────────────────────────────────────────────────────────*/

/** Y where the sidebar and the column-header row begin. */
static int content_top(void)
{
	return TOOLBAR_H + ADDR_H;
}

/** Left X of the list content (right of the sidebar). */
static int content_x(void)
{
	return SIDEBAR_W;
}

/** Y of the first list row.  Details has a column header; Icons does not. */
static int list_top(void)
{
	return content_top() + (g_view == VIEW_ICONS ? 0 : HEAD_H);
}

/** X of the Name column / entry labels (Details view). */
static int name_x(void)
{
	return content_x() + PAD + ICON_W + 6;
}

/** Height of one scroll unit: a row (Details) or a cell row (Icons). */
static int unit_h(void)
{
	return g_view == VIEW_ICONS ? CELL_H : ROW_H;
}

/** Number of icon columns across the content area (1 in Details). */
static int grid_cols(void)
{
	if (g_view != VIEW_ICONS)
		return 1;
	int avail = g_w - content_x() - SCROLLBAR_W;
	int c = avail / CELL_W;
	return c > 0 ? c : 1;
}

/** Total scroll units: entry rows (Details) or cell rows (Icons). */
static int total_units(void)
{
	int cols = grid_cols();
	return (g_nent + cols - 1) / cols;
}

/** Number of fully-visible scroll units for the current window height. */
static int visible_rows(void)
{
	int h = g_h - STATUS_H - list_top();
	return h > 0 ? h / unit_h() : 0;
}

/** Largest valid scroll offset (0 when everything fits). */
static int max_top(void)
{
	int m = total_units() - visible_rows();
	return m > 0 ? m : 0;
}

/** Clamp g_top into [0, max_top()]. */
static void clamp_top(void)
{
	if (g_top > max_top())
		g_top = max_top();
	if (g_top < 0)
		g_top = 0;
}

/** True when the content is taller than the viewport and needs a scrollbar. */
static bool scrollbar_visible(void)
{
	return total_units() > visible_rows();
}

/** Compute the scrollbar thumb's top Y (*ty) and height (*th). */
static void sb_thumb(int *ty, int *th)
{
	int y0 = list_top();
	int track = (g_h - STATUS_H) - y0;
	int units = total_units();
	int t = (units > 0) ? track * visible_rows() / units : track;
	if (t < SCROLL_THUMB_MIN)
		t = SCROLL_THUMB_MIN;
	if (t > track)
		t = track;
	int mt = max_top();
	*ty = y0 + ((mt > 0) ? (track - t) * g_top / mt : 0);
	*th = t;
}

/** Entry index under (x,y) in the list area, or -1.  Handles both view modes. */
static int list_hit(int x, int y)
{
	if (y < list_top() || y >= g_h - STATUS_H || x < content_x())
		return -1;
	if (g_view == VIEW_ICONS)
	{
		int cols = grid_cols();
		int col = (x - content_x()) / CELL_W;
		if (col < 0 || col >= cols)
			return -1;
		int i = (g_top + (y - list_top()) / CELL_H) * cols + col;
		return (i >= 0 && i < g_nent) ? i : -1;
	}
	int i = g_top + (y - list_top()) / ROW_H;
	return (i >= 0 && i < g_nent) ? i : -1;
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
 * Draw a large (BIG_ICON-wide) folder or document glyph at (x,y), for icon view.
 */
static void draw_big_icon(int x, int y, bool is_dir)
{
	if (is_dir)
	{
		g_surf.ogFillRect(x, y + 10, x + BIG_ICON - 1, y + 38, COL_FOLDER);
		g_surf.ogFillRect(x, y + 4, x + 18, y + 12, COL_FOLDER); /* tab */
	}
	else
	{
		g_surf.ogFillRect(x + 6, y + 2, x + BIG_ICON - 6, y + 40, COL_FILE);
		g_surf.ogFillRect(x + BIG_ICON - 18, y + 2, x + BIG_ICON - 6, y + 14, COL_FILE_FOLD); /* fold */
	}
}

/**
 * Copy @name into @out, truncating with a ".." tail if it would exceed @maxw
 * pixels in the current font.
 */
static void fit_name(const char *name, int maxw, char *out, size_t outsz)
{
	snprintf(out, outsz, "%s", name);
	if ((int)g_font.TextWidth(out) <= maxw)
		return;
	size_t n = strlen(out);
	while (n > 1)
	{
		out[--n] = '\0';
		char probe[NAME_MAX_LEN + 4];
		snprintf(probe, sizeof(probe), "%s..", out);
		if ((int)g_font.TextWidth(probe) <= maxw)
		{
			snprintf(out, outsz, "%s", probe);
			return;
		}
	}
}

/**
 * Draw a 16px "home" glyph (a roof over a body) at (x,y).
 */
static void draw_home_icon(int x, int y)
{
	for (int i = 0; i < 7; i++) /* triangular roof */
		g_surf.ogHLine(x + 7 - i, x + 7 + i, y + 1 + i, COL_FOLDER);
	g_surf.ogFillRect(x + 2, y + 7, x + 12, y + 14, COL_FOLDER);
}

/**
 * Draw the left navigation sidebar: a "Places" header over the quick-access rows
 * (Home, Filesystem, top-level folders).  The row matching the current directory
 * is highlighted.
 */
static void draw_sidebar(void)
{
	int top = content_top();
	int bottom = g_h - STATUS_H;
	g_surf.ogFillRect(0, top, SIDEBAR_W - 1, bottom - 1, COL_HEAD);
	g_surf.ogVLine(SIDEBAR_W - 1, top, bottom - 1, COL_DIVIDER);

	set_fg(COL_TEXT_DIM);
	g_font.PutString(g_surf, PAD, top + 6, "PLACES");

	int y = top + 6 + (int)g_font.GetHeight() + 6;
	for (int i = 0; i < g_nplaces && y + ROW_H <= bottom; i++)
	{
		bool here = (strcmp(g_places[i].path, g_cwd) == 0);
		if (here)
			g_surf.ogFillRect(3, y, SIDEBAR_W - 4, y + ROW_H - 1, COL_SEL);

		if (g_places[i].is_home)
			draw_home_icon(PAD, y + (ROW_H - 16) / 2);
		else
			draw_icon(PAD, y + (ROW_H - 16) / 2, true);

		set_fg(here ? COL_SEL_TEXT : COL_TEXT);
		g_font.PutString(
		    g_surf, PAD + ICON_W + 6, y + (ROW_H - (int)g_font.GetHeight()) / 2, g_places[i].label);
		y += ROW_H;
	}
}

/**
 * Index of the sidebar place at (x,y), or -1 if the point is outside the list.
 */
static int sidebar_hit(int x, int y)
{
	if (x >= SIDEBAR_W)
		return -1;
	int first = content_top() + 6 + (int)g_font.GetHeight() + 6;
	if (y < first)
		return -1;
	int i = (y - first) / ROW_H;
	return (i >= 0 && i < g_nplaces) ? i : -1;
}

/**
 * Draw the navigation toolbar (Back / Forward / Up) and lay out / hit-enable its
 * buttons.  Back/Forward gate on history position, Up on not being at root.
 */
static void draw_toolbar(void)
{
	g_surf.ogFillRect(0, 0, g_w - 1, TOOLBAR_H - 1, COL_TOOLBAR);
	g_surf.ogHLine(0, g_w - 1, TOOLBAR_H - 1, COL_DIVIDER);

	bool can_back = (g_hist_pos > 0);
	bool can_fwd = (g_hist_pos + 1 < g_hist_len);

	int bx = 8;
	for (int i = 0; i < BTN_COUNT; i++)
	{
		g_btn[i].x = bx;
		g_btn[i].y = 7;
		g_btn[i].w = (i == BTN_UP) ? 40 : 34;
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
			default:
				g_btn[i].enabled = true;
				break;
		}
		g_btn[i].Draw(g_surf, g_font);
		bx += g_btn[i].w + 6;
	}

	/* View-mode toggle (Details / Icons): the active mode is drawn pressed. */
	static const char *vlabel[2] = {"Details", "Icons"};
	bx += 8;
	for (int v = 0; v < 2; v++)
	{
		g_vbtn[v].x = bx;
		g_vbtn[v].y = 7;
		g_vbtn[v].w = (int)g_font.TextWidth(vlabel[v]) + 18;
		g_vbtn[v].h = TOOLBAR_H - 14;
		g_vbtn[v].label = vlabel[v];
		g_vbtn[v].enabled = true;
		g_vbtn[v].Draw(g_surf, g_font, g_view == v);
		bx += g_vbtn[v].w + 4;
	}

	/* Filter box on the right: a rounded field showing the filter text (or a dim
	 * "Filter" placeholder).  Click it to type; matches narrow the list live. */
	g_filter_w = 180;
	g_filter_x = g_w - PAD - g_filter_w;
	int fy = 7, fh = TOOLBAR_H - 14;
	g_surf.ogFillRoundRect(g_filter_x, fy, g_filter_x + g_filter_w, fy + fh, 4, COL_ADDR);
	g_surf.ogRoundRect(g_filter_x, fy, g_filter_x + g_filter_w, fy + fh, 4, g_filter_edit ? COL_SEL : COL_DIVIDER);
	int fty = fy + (fh - (int)g_font.GetHeight()) / 2;
	if (g_filter_edit || g_filter[0])
	{
		set_fg(COL_TEXT);
		char shown[80];
		snprintf(shown, sizeof(shown), "%s%s", g_filter, g_filter_edit ? "|" : "");
		g_font.PutString(g_surf, g_filter_x + 8, fty, shown);
	}
	else
	{
		set_fg(COL_TEXT_DIM);
		g_font.PutString(g_surf, g_filter_x + 8, fty, "Filter");
	}
}

/**
 * Draw the address bar.  In browse mode it shows clickable breadcrumb segments
 * (recorded in g_crumb for hit-testing); in edit mode it shows an editable path
 * field with a caret.
 */
static void draw_address(void)
{
	int ay = TOOLBAR_H;
	g_surf.ogFillRect(0, ay, g_w - 1, ay + ADDR_H - 1, COL_ADDR);
	g_surf.ogHLine(0, g_w - 1, ay + ADDR_H - 1, COL_DIVIDER);

	int ety = ay + (ADDR_H - (int)g_font.GetHeight()) / 2;

	/* Editable path field. */
	if (g_addr_edit)
	{
		g_surf.ogRoundRect(PAD - 4, ay + 4, g_w - PAD, ay + ADDR_H - 5, 3, COL_SEL);
		set_fg(COL_TEXT);
		char shown[520];
		snprintf(shown, sizeof(shown), "%s|", g_addr_buf);
		g_font.PutString(g_surf, PAD, ety, shown);
		return;
	}

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
 * Draw one sortable column header label, with an ascending/descending caret when
 * it is the active sort key.  ASCII carets (not Unicode arrows) because the font
 * renderer is byte-indexed, not UTF-8 aware.
 */
static void draw_head_label(int x, const char *label, int col)
{
	set_fg(col == g_sort ? COL_TEXT : COL_TEXT_DIM);
	char buf[48];
	if (col == g_sort)
		snprintf(buf, sizeof(buf), "%s %s", label, g_sort_desc ? "v" : "^");
	else
		snprintf(buf, sizeof(buf), "%s", label);
	g_font.PutString(g_surf, x, TOOLBAR_H + ADDR_H + (HEAD_H - (int)g_font.GetHeight()) / 2, buf);
}

/**
 * Draw the column-header row (Name / Size / Type / Modified).
 */
static void draw_header(void)
{
	if (g_view == VIEW_ICONS)
		return; /* icon grid has no column header */
	int hy = TOOLBAR_H + ADDR_H;
	g_surf.ogFillRect(content_x(), hy, g_w - 1, hy + HEAD_H - 1, COL_HEAD);
	g_surf.ogHLine(content_x(), g_w - 1, hy + HEAD_H - 1, COL_DIVIDER);
	draw_head_label(name_x(), "Name", SORT_NAME);
	draw_head_label(col_size_r() - (int)g_font.TextWidth("Size") - 14, "Size", SORT_SIZE);
	draw_head_label(col_type_x(), "Type", SORT_TYPE);
	draw_head_label(col_mod_x(), "Modified", SORT_MOD);
}

/**
 * Draw the visible slice of the file list, including row striping, the selection
 * highlight, and the inline rename editor when active.
 */
/**
 * Draw the icon-grid view: large icons in a wrapping grid, each with a centered,
 * truncated name caption.
 */
static void draw_icons(void)
{
	int top = list_top();
	int bottom = g_h - STATUS_H;
	int cx = content_x();
	g_surf.ogFillRect(cx, top, g_w - 1, bottom - 1, COL_WIN);

	int cols = grid_cols();
	for (int i = g_top * cols; i < g_nent; i++)
	{
		int rel_row = i / cols - g_top;
		int y = top + rel_row * CELL_H;
		if (y >= bottom)
			break;
		int x = cx + (i % cols) * CELL_W;
		struct fentry *e = &g_ent[i];
		bool sel = (i == g_sel);

		if (sel)
			g_surf.ogFillRoundRect(x + 4, y + 4, x + CELL_W - 4, y + CELL_H - 4, 6, COL_SEL);

		draw_big_icon(x + (CELL_W - BIG_ICON) / 2, y + 12, e->is_dir);

		char label[NAME_MAX_LEN + 4];
		fit_name(e->name, CELL_W - 12, label, sizeof(label));
		set_fg(sel ? COL_SEL_TEXT : COL_TEXT);
		int tw = (int)g_font.TextWidth(label);
		g_font.PutString(g_surf, x + (CELL_W - tw) / 2, y + 12 + BIG_ICON + 8, label);
	}
}

static void draw_list(void)
{
	clamp_top();
	if (g_view == VIEW_ICONS)
	{
		draw_icons();
		return;
	}
	int top = list_top();
	int bottom = g_h - STATUS_H;
	int cx = content_x();
	g_surf.ogFillRect(cx, top, g_w - 1, bottom - 1, COL_WIN);

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
			g_surf.ogFillRect(cx, y, g_w - 1, y + ROW_H - 1, COL_SEL);
		else if (i & 1)
			g_surf.ogFillRect(cx, y, g_w - 1, y + ROW_H - 1, COL_ROW_ALT);

		draw_icon(cx + PAD, y + (ROW_H - 16) / 2, e->is_dir);

		int ty = y + (ROW_H - (int)g_font.GetHeight()) / 2;
		uint32_t fg = sel ? COL_SEL_TEXT : COL_TEXT;
		uint32_t dim = sel ? COL_SEL_TEXT : COL_TEXT_DIM;

		/* Name (inline editor for the row being renamed). */
		if (g_mode == MODE_RENAME && sel)
		{
			int ex = name_x();
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
			g_font.PutString(g_surf, name_x(), ty, e->name);
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
 * Draw the vertical scrollbar on the right edge of the list (track + a
 * proportional thumb), only when the content overflows the viewport.
 */
static void draw_scrollbar(void)
{
	if (!scrollbar_visible())
		return;
	int x0 = g_w - SCROLLBAR_W;
	g_surf.ogFillRect(x0, list_top(), g_w - 1, (g_h - STATUS_H) - 1, COL_ROW_ALT);
	int ty, th;
	sb_thumb(&ty, &th);
	g_surf.ogFillRoundRect(x0 + 2, ty + 1, g_w - 3, ty + th - 1, 3, COL_TEXT_DIM);
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

	/* Free space, centered (only when no selection occupies the right side). */
	if (g_free_str[0])
		g_font.PutString(g_surf, (g_w - (int)g_font.TextWidth(g_free_str)) / 2, ty, g_free_str);

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

	g_dlg_ok.x = dx + dw - 100;
	g_dlg_ok.y = dy + dh - 40;
	g_dlg_ok.w = 84;
	g_dlg_ok.h = 28;
	g_dlg_ok.label = "Delete";
	g_dlg_ok.enabled = true;
	g_dlg_ok.Draw(g_surf, g_font);

	g_dlg_cancel.x = dx + dw - 196;
	g_dlg_cancel.y = dy + dh - 40;
	g_dlg_cancel.w = 84;
	g_dlg_cancel.h = 28;
	g_dlg_cancel.label = "Cancel";
	g_dlg_cancel.enabled = true;
	g_dlg_cancel.Draw(g_surf, g_font);
}

/**
 * Build the context menu for a right-click at (mx,my): an item menu (Open /
 * Rename / Delete) when over a row, else a folder-background menu (New Folder /
 * Refresh).  Positions and clamps the menu within the window.
 */
static void open_context_menu(int mx, int my)
{
	g_ctx_n = 0;
	g_ctx_target = -1;

	/* Did the click land on a list item (row or icon cell)? */
	int hit = list_hit(mx, my);
	if (hit >= 0)
	{
		g_sel = hit;
		g_ctx_target = hit;
	}

	if (g_ctx_target >= 0)
	{
		g_ctx_items[g_ctx_n++] = {"Open", ACT_OPEN, true};
		g_ctx_items[g_ctx_n++] = {"Rename", ACT_RENAME, false};
		g_ctx_items[g_ctx_n++] = {"Delete", ACT_DELETE, false};
	}
	else
	{
		g_ctx_items[g_ctx_n++] = {"New Folder", ACT_NEWFOLDER, true};
		g_ctx_items[g_ctx_n++] = {"Refresh", ACT_REFRESH, false};
	}

	g_ctx_w = CTX_W;
	g_ctx_h = g_ctx_n * CTX_ROW_H + 8;
	g_ctx_x = mx;
	g_ctx_y = my;
	if (g_ctx_x + g_ctx_w > g_w)
		g_ctx_x = g_w - g_ctx_w - 2;
	if (g_ctx_y + g_ctx_h > g_h)
		g_ctx_y = g_h - g_ctx_h - 2;
	if (g_ctx_x < 0)
		g_ctx_x = 0;
	if (g_ctx_y < 0)
		g_ctx_y = 0;
	g_ctx_open = true;
}

/**
 * Run a context-menu action against the current target row / directory, then
 * close the menu.
 */
static void run_context_action(int action)
{
	g_ctx_open = false;
	switch (action)
	{
		case ACT_OPEN:
			if (g_ctx_target >= 0)
				open_entry(g_ctx_target);
			break;
		case ACT_NEWFOLDER:
			new_folder();
			break;
		case ACT_RENAME:
			start_rename();
			break;
		case ACT_DELETE:
			if (g_sel >= 0)
				g_mode = MODE_CONFIRM_DEL;
			break;
		case ACT_REFRESH:
			read_dir();
			snprintf(g_status, sizeof(g_status), "%d item%s", g_nent, g_nent == 1 ? "" : "s");
			update_free_space();
			break;
		default:
			break;
	}
}

/**
 * Draw the open context menu as a rounded card with one row per item and a thin
 * separator under the primary action.
 */
static void draw_context_menu(void)
{
	g_surf.ogFillRoundRect(g_ctx_x, g_ctx_y, g_ctx_x + g_ctx_w, g_ctx_y + g_ctx_h, 6, COL_ADDR);
	g_surf.ogRoundRect(g_ctx_x, g_ctx_y, g_ctx_x + g_ctx_w, g_ctx_y + g_ctx_h, 6, COL_DIVIDER);

	for (int i = 0; i < g_ctx_n; i++)
	{
		int ry = g_ctx_y + 4 + i * CTX_ROW_H;
		set_fg(COL_TEXT);
		g_font.PutString(
		    g_surf, g_ctx_x + 14, ry + (CTX_ROW_H - (int)g_font.GetHeight()) / 2, g_ctx_items[i].label);
		if (g_ctx_items[i].separator_after && i + 1 < g_ctx_n)
			g_surf.ogHLine(g_ctx_x + 8, g_ctx_x + g_ctx_w - 8, ry + CTX_ROW_H - 1, COL_DIVIDER);
	}
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
	draw_scrollbar();
	draw_sidebar();
	draw_header();
	draw_address();
	draw_toolbar();
	draw_status();
	if (g_ctx_open)
		draw_context_menu();
	if (g_mode == MODE_CONFIRM_DEL)
		draw_confirm();
	flip();
}

/**
 * Index of the context-menu row at (x,y), or -1 if the point is outside the menu.
 */
static int ctx_hit(int x, int y)
{
	if (x < g_ctx_x || x >= g_ctx_x + g_ctx_w || y < g_ctx_y + 4 || y >= g_ctx_y + g_ctx_h)
		return -1;
	int i = (y - (g_ctx_y + 4)) / CTX_ROW_H;
	return (i >= 0 && i < g_ctx_n) ? i : -1;
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
	int sel_unit = (g_view == VIEW_ICONS) ? g_sel / grid_cols() : g_sel; /* scroll-unit row */
	if (sel_unit < g_top)
		g_top = sel_unit;
	else if (sel_unit >= g_top + rows)
		g_top = sel_unit - rows + 1;
	if (g_top < 0)
		g_top = 0;
}

/**
 * Handle a left-button press at (x,y): an open context menu (if any) takes
 * priority, then toolbar buttons, breadcrumb segments, column headers, and row
 * select / double-click-to-open.
 */
static void on_click(int x, int y)
{
	/* An open context menu consumes the click: run the hit item, else dismiss. */
	if (g_ctx_open)
	{
		int hit = ctx_hit(x, y);
		if (hit >= 0)
			run_context_action(g_ctx_items[hit].action);
		else
			g_ctx_open = false;
		render();
		return;
	}

	/* Editing the address bar: a click outside the bar cancels the edit. */
	if (g_addr_edit && !(y >= TOOLBAR_H && y < TOOLBAR_H + ADDR_H))
	{
		g_addr_edit = false;
		render();
		return;
	}

	/* Editing the filter: a click outside the filter box ends the edit. */
	if (g_filter_edit && !(y < TOOLBAR_H && x >= g_filter_x && x <= g_filter_x + g_filter_w))
	{
		g_filter_edit = false;
		render();
		return;
	}

	/* Modal: only the dialog buttons are live. */
	if (g_mode == MODE_CONFIRM_DEL)
	{
		if (g_dlg_ok.Hit(x, y))
		{
			g_mode = MODE_BROWSE;
			do_delete();
		}
		else if (g_dlg_cancel.Hit(x, y))
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
		if (x >= g_filter_x && x <= g_filter_x + g_filter_w)
		{
			g_filter_edit = true; /* click the filter box to type */
			g_addr_edit = false;
			render();
			return;
		}
		if (g_vbtn[0].Hit(x, y))
			g_view = VIEW_DETAILS;
		else if (g_vbtn[1].Hit(x, y))
			g_view = VIEW_ICONS;
		else if (g_btn[BTN_BACK].Hit(x, y) && g_hist_pos > 0)
			navigate(g_hist[--g_hist_pos], false);
		else if (g_btn[BTN_FWD].Hit(x, y) && g_hist_pos + 1 < g_hist_len)
			navigate(g_hist[++g_hist_pos], false);
		else if (g_btn[BTN_UP].Hit(x, y))
		{
			char up[512];
			parent_of(g_cwd, up, sizeof(up));
			navigate(up, true);
		}
		clamp_top();
		render();
		return;
	}

	/* Address bar.  Already editing: a click in the bar keeps the edit.  A click
	 * on a breadcrumb segment navigates; a click on the empty part of the bar
	 * starts editing the path (Explorer: click the address to type). */
	if (y >= TOOLBAR_H && y < TOOLBAR_H + ADDR_H)
	{
		if (g_addr_edit)
			return;
		for (int i = 0; i < g_ncrumb; i++)
			if (x >= g_crumb[i].x0 - 2 && x <= g_crumb[i].x1 + 2)
			{
				navigate(g_crumb[i].path, true);
				render();
				return;
			}
		addr_begin_edit();
		render();
		return;
	}

	/* Sidebar place → jump there. */
	if (x < SIDEBAR_W && y >= content_top())
	{
		int p = sidebar_hit(x, y);
		if (p >= 0)
			navigate(g_places[p].path, true);
		render();
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

	/* Scrollbar: grab the thumb to drag, or click the track to page. */
	if (scrollbar_visible() && x >= g_w - SCROLLBAR_W && y >= list_top() && y < g_h - STATUS_H)
	{
		int ty, th;
		sb_thumb(&ty, &th);
		if (y < ty)
			g_top -= visible_rows();
		else if (y >= ty + th)
			g_top += visible_rows();
		else
		{
			g_scroll_drag = true;
			g_scroll_drag_off = y - ty;
		}
		clamp_top();
		render();
		return;
	}

	/* List item (row in Details, cell in Icons). */
	if (y >= list_top() && y < g_h - STATUS_H)
	{
		int i = list_hit(x, y);
		if (i < 0)
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
 * Handle a right-button press at (x,y): open the context menu (over a row → item
 * actions; over the background → folder actions).  Ignored while modal.
 */
static void on_right_click(int x, int y)
{
	if (g_mode != MODE_BROWSE)
		return;
	if (x < SIDEBAR_W && y >= content_top())
		return; /* no context menu in the sidebar */
	open_context_menu(x, y);
	render();
}

/**
 * Handle pointer motion while the left button is held: drives a scrollbar-thumb
 * drag (mapping the cursor's track position back to a scroll offset).
 */
static void on_drag(int y)
{
	if (!g_scroll_drag)
		return;
	int y0 = list_top();
	int track = (g_h - STATUS_H) - y0;
	int ty, th;
	sb_thumb(&ty, &th);
	int span = track - th;
	int mt = max_top();
	if (span <= 0 || mt <= 0)
		return;
	int rel = (y - g_scroll_drag_off) - y0;
	if (rel < 0)
		rel = 0;
	if (rel > span)
		rel = span;
	g_top = rel * mt / span;
	clamp_top();
	render();
}

/**
 * Handle left-button release: end any in-progress scrollbar drag.
 */
static void on_release(void)
{
	g_scroll_drag = false;
}

/**
 * Handle a key press.  Routes to the rename editor or delete dialog when modal;
 * otherwise drives selection, navigation, type-ahead, and the action shortcuts.
 */
static void on_key(uint32_t kc)
{
	/* Editing the address bar: type a path, Enter to go, Esc to cancel. */
	if (g_addr_edit)
	{
		if (kc == '\r' || kc == '\n')
			addr_commit();
		else if (kc == KEY_ESC)
			g_addr_edit = false;
		else if (kc == '\b' || kc == 0x7F)
		{
			if (g_addr_len > 0)
				g_addr_buf[--g_addr_len] = '\0';
		}
		else if (kc >= 0x20 && kc < 0x7F && g_addr_len < (int)sizeof(g_addr_buf) - 1)
		{
			g_addr_buf[g_addr_len++] = (char)kc;
			g_addr_buf[g_addr_len] = '\0';
		}
		render();
		return;
	}

	/* Editing the filter: each keystroke re-narrows the list live.  Enter keeps
	 * the filter and leaves the box; Esc clears it. */
	if (g_filter_edit)
	{
		bool changed = false;
		if (kc == '\r' || kc == '\n')
			g_filter_edit = false;
		else if (kc == KEY_ESC)
		{
			g_filter[0] = '\0';
			g_filter_len = 0;
			g_filter_edit = false;
			changed = true;
		}
		else if (kc == '\b' || kc == 0x7F)
		{
			if (g_filter_len > 0)
			{
				g_filter[--g_filter_len] = '\0';
				changed = true;
			}
		}
		else if (kc >= 0x20 && kc < 0x7F && g_filter_len < (int)sizeof(g_filter) - 1)
		{
			g_filter[g_filter_len++] = (char)kc;
			g_filter[g_filter_len] = '\0';
			changed = true;
		}
		if (changed)
		{
			read_dir();
			snprintf(g_status, sizeof(g_status), "%d item%s", g_nent, g_nent == 1 ? "" : "s");
		}
		render();
		return;
	}

	/* An open context menu: Escape dismisses it. */
	if (g_ctx_open)
	{
		if (kc == KEY_ESC)
		{
			g_ctx_open = false;
			render();
		}
		return;
	}

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
		{
			int step = (g_view == VIEW_ICONS) ? grid_cols() : 1; /* one grid row up */
			if (g_sel < 0 && g_nent > 0)
				g_sel = 0;
			else if (g_sel - step >= 0)
				g_sel -= step;
			ensure_visible();
			break;
		}
		case KEY_DOWN:
		{
			int step = (g_view == VIEW_ICONS) ? grid_cols() : 1;
			if (g_sel + step < g_nent)
				g_sel += step;
			ensure_visible();
			break;
		}
		case KEY_LEFT:
			if (g_view == VIEW_ICONS && g_sel > 0)
			{
				g_sel--;
				ensure_visible();
			}
			break;
		case KEY_RIGHT:
			if (g_view == VIEW_ICONS && g_sel < g_nent - 1)
			{
				g_sel++;
				ensure_visible();
			}
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
	req->min_w = 600; /* sidebar + a usable list */
	req->min_h = 320;
	req->max_w = 1200;
	req->max_h = 900;
	req->wants_motion = 1; /* deliver pointer motion so the scrollbar thumb drags */
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

	build_places();
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
				/* Motion events arrive too (wants_motion), so act on button edges:
				 * a fresh press is a click, button-held motion is a drag, and a
				 * release ends a drag.  Hover (no button) is ignored. */
				struct display_mouse_ev *me = (struct display_mouse_ev *)ev.data;
				if (me->wheel != 0 && g_mode == MODE_BROWSE && !g_ctx_open)
				{
					g_top -= me->wheel * 3; /* one notch ≈ 3 rows */
					clamp_top();
					render();
				}
				uint8_t b = me->buttons;
				bool lnow = (b & 1) != 0, lprev = (g_prev_buttons & 1) != 0;
				bool rnow = (b & 2) != 0, rprev = (g_prev_buttons & 2) != 0;
				if (rnow && !rprev)
					on_right_click(me->x, me->y);
				else if (lnow && !lprev)
					on_click(me->x, me->y);
				else if (lnow && lprev)
					on_drag(me->y);
				else if (!lnow && lprev)
					on_release();
				g_prev_buttons = b;
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
