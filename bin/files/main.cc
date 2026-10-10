/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Files — the uBixOS graphical file manager (P0 MVP).
 *
 * An Explorer-leaning, best-of-all-worlds file browser: a navigation toolbar
 * (back / forward / up), a breadcrumb address bar, a left "Places" sidebar (Home,
 * Filesystem, top-level folders), a sortable Details list (Name / Size / Type /
 * Modified), and a status bar.  Double-click a folder to
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

	extern char **environ; /* inherited session env, forwarded to opened apps */
}
#include <objgfx/objgfx.h>
#include <objgfx/ogScalableFont.h>
#include <objgfx/ogButton.h>
#include <views/theme.h>
#include <views/uidraw.h>
#include <api/ubfs_pool.h>

#define RGB(r, g, b) ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))

#define WIN_W 980
#define WIN_H 620
#define TOOLBAR_H 44  /* buttons, breadcrumb, search, view toggle (design) */
#define ADDR_H 0      /* the breadcrumb now lives in the toolbar */
#define SIDEBAR_W 200 /* left navigation sidebar */
#define HEAD_H 30     /* column-header row (list view) */
#define ROW_H 28      /* one list / sidebar row */
#define STATUS_H 24
#define PAD 10
#define ICON_W 18
#define FONT_PATH "/var/fonts/DejaVuSans.ttf"

#define MAX_ENTRIES 1024
#define MAX_CRUMBS 64
#define MAX_PLACES 24
#define NAME_MAX_LEN 256

/* Palette — matches the diskutil / activity desktop look. */
/* Colours follow the system theme (views/theme.h): Files is a regular app,
 * so it honours light/dark; g_t is (re)loaded at start-up. */
static struct ui_theme g_t = ui_theme_dark;
#define COL_WIN (g_t.ground)
#define COL_TOOLBAR (g_t.ground)
#define COL_ADDR (g_t.surface)
#define COL_HEAD (g_t.surface)
#define COL_SEL (g_t.accent_900)
#define COL_SEL_TEXT (g_t.accent_200)
#define COL_ROW_ALT (ui_mix(g_t.ground, g_t.surface, 96))
#define COL_DIVIDER (ui_mix(g_t.ground, g_t.divider, g_t.divider_a))
#define COL_TEXT (g_t.text)
#define COL_TEXT_DIM (g_t.text_muted)
#define COL_FOLDER (g_t.accent_400)
#define COL_FILE (g_t.text_faint)
#define COL_FILE_FOLD (g_t.text_muted)
#define COL_CRUMB (g_t.accent_300)

static ogSurface g_surf; /* the window buffer */

/**
 * The window buffer as a ui_canvas (views/uidraw.h drawing).
 */
static ui_canvas files_canvas(void)
{
	ui_canvas c;
	c.px = (uint32_t *)g_surf.ogGetPtr(0, 0);
	c.w = (int)g_surf.ogGetMaxX() + 1;
	c.h = (int)g_surf.ogGetMaxY() + 1;
	c.stride = (int)((uint8_t *)g_surf.ogGetPtr(0, 1) - (uint8_t *)g_surf.ogGetPtr(0, 0)) / 4;
	return c;
}

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

static ogScalableFont g_font;    /* 12px UI text */
static ogScalableFont g_font_sm; /* 11px: tile labels, header, status */
static ogScalableFont g_font_xs; /* 10px: section labels, meta */

/* View mode and the search filter (toolbar). */
enum
{
	VIEW_GRID = 0,
	VIEW_LIST = 1
};
static int g_view = VIEW_GRID;
static char g_query[64];    /* case-insensitive name filter for the current folder */
static bool g_search_focus; /* typing goes to the search field */
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

static char g_status[160];

/* Double-click tracking. */
static int g_last_click_row = -1;
static int64_t g_last_click_ms;

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
static const char *g_btn_icon[BTN_COUNT] = {"arrow-left", "arrow-right", "arrow-up"};

/* Confirmation-dialog buttons (own objects so they don't alias the toolbar). */
static ogButton g_dlg_ok;
static ogButton g_dlg_cancel;

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
/* Case-insensitive substring match for the search filter. */
static bool name_matches(const char *name, const char *q)
{
	for (; *name; name++)
	{
		const char *a = name, *b = q;
		while (*a && *b && ((*a | 0x20) == (*b | 0x20)))
			a++, b++;
		if (*b == '\0')
			return true;
	}
	return false;
}

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
		if (g_query[0] && !name_matches(de->d_name, g_query))
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
	if (g_query[0])
		snprintf(g_status, sizeof(g_status), "%d match%s", g_nent, g_nent == 1 ? "" : "es");
	else
		snprintf(g_status, sizeof(g_status), "%d item%s", g_nent, g_nent == 1 ? "" : "s");
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
	g_query[0] = '\0'; /* a new folder starts unfiltered */
	if (read_dir() == 0)
		snprintf(g_status, sizeof(g_status), "%d item%s", g_nent, g_nent == 1 ? "" : "s");
	/* On failure read_dir leaves an explanatory message in g_status. */
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

/* ── layout queries ─────────────────────────────────────────────────────────*/

/** Y where the sidebar and the content begin (below the toolbar). */
static int content_top(void)
{
	return TOOLBAR_H + ADDR_H;
}

/** Y of the first list row (list view: below the column header). */
static int list_top(void)
{
	return content_top() + HEAD_H;
}

/** Left X of the content (right of the sidebar). */
static int content_x(void)
{
	return SIDEBAR_W;
}

/** X of the Name column / entry labels (list view). */
static int name_x(void)
{
	return content_x() + PAD + ICON_W + 8;
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

/* Grid view (design: 112x100 tiles, 4px gap, content padding 12 / 16). */
#define TILE_W 112
#define TILE_H 100
#define TILE_GAP 4
#define GRID_PX 16
#define GRID_PY 12

/** Tiles per grid row for the current width. */
static int grid_cols(void)
{
	int w = g_w - content_x() - 2 * GRID_PX + TILE_GAP;
	int c = w / (TILE_W + TILE_GAP);
	return c > 0 ? c : 1;
}

/** Fully-visible grid rows for the current height. */
static int grid_rows(void)
{
	int h = g_h - STATUS_H - content_top() - GRID_PY;
	int r = h / TILE_H;
	return r > 0 ? r : 1;
}

/** Entry index of the grid tile at (x,y), or -1. */
static int grid_hit(int x, int y)
{
	int gx = x - content_x() - GRID_PX, gy = y - content_top() - GRID_PY;
	if (gx < 0 || gy < 0)
		return -1;
	int c = gx / (TILE_W + TILE_GAP), r = gy / TILE_H;
	if (c >= grid_cols() || gx % (TILE_W + TILE_GAP) >= TILE_W)
		return -1;
	int i = (g_top + r) * grid_cols() + c;
	return (i < g_nent) ? i : -1;
}

/* ── drawing ────────────────────────────────────────────────────────────────*/

/** Text left-aligned at x, vertically centred on row cy. */
static void text_at(ogScalableFont &f, int x, int cy, const char *s, uint32_t col)
{
	ui_text(g_surf, f, x, cy, s, col);
}

/**
 * The design's 48px file-type icon for an entry, by kind / extension.
 */
static const char *file_icon48(const struct fentry *e)
{
	if (e->is_dir)
		return "file-folder";
	char ext[16];
	ext_of(e->name, ext, sizeof(ext));
	static const struct
	{
		const char *exts, *icon;
	} map[] = {
	    {"|pdf|", "file-pdf"},
	    {"|doc|docx|odt|rtf|", "file-doc"},
	    {"|xls|xlsx|ods|csv|", "file-sheet"},
	    {"|tar|gz|tgz|zip|bz2|xz|z|", "file-archive"},
	    {"|png|jpg|jpeg|bmp|gif|ppm|", "file-image"},
	    {"|mp3|wav|flac|ogg|mod|", "file-audio"},
	    {"|c|h|cc|cpp|hh|hpp|s|py|js|sh|mk|asm|", "file-code"},
	};
	if (ext[0])
	{
		char key[24];
		snprintf(key, sizeof(key), "|%s|", ext);
		for (auto &m : map)
			if (strstr(m.exts, key))
				return m.icon;
	}
	return "file-text";
}

/**
 * Draw a 16px folder or document glyph at (x,y) (list view, sidebar).
 */
static void draw_icon(int x, int y, bool is_dir)
{
	ui_canvas cv = files_canvas();
	if (is_dir)
		ui_draw_icon_tint(cv, ui_icon_get(16, "folder"), x, y, COL_FOLDER);
	else
		ui_draw_icon_tint(cv, ui_icon_get(16, "list"), x, y, COL_TEXT_DIM);
}

/**
 * The 16px glyph for a sidebar place (Home, the filesystem, top-level dirs).
 */
static const char *place_icon(const struct place *p)
{
	if (p->is_home)
		return "home";
	static const struct
	{
		const char *path, *icon;
	} map[] = {{"/", "drive"},
	           {"/bin", "terminal"},
	           {"/boot", "power"},
	           {"/etc", "sliders"},
	           {"/lib", "code"},
	           {"/usr", "folder"},
	           {"/var", "folder"},
	           {"/home", "home"},
	           {"/tmp", "trash"}};
	for (auto &m : map)
		if (strcmp(p->path, m.path) == 0)
			return m.icon;
	return "folder";
}

/* Section label: 10px, upper-case, faint (design .section-label). */
static void section_label(int x, int cy, const char *s)
{
	char up[32];
	int i = 0;
	for (; s[i] && i < (int)sizeof(up) - 1; i++)
		up[i] = (char)((s[i] >= 'a' && s[i] <= 'z') ? s[i] - 32 : s[i]);
	up[i] = '\0';
	text_at(g_font_xs, x, cy, up, g_t.text_faint);
}

/* Sidebar geometry: rows start below the "Places" label. */
#define SB_ROW_Y0 (content_top() + 8 + 24)

/**
 * Draw the sidebar: Places (Home, Filesystem, top-level folders) as list rows
 * with icons, the current one selected; then Devices — the mounted UbixFS pool
 * with a usage bar (from ubix_pool_query).
 */
static void draw_sidebar(void)
{
	int top = content_top();
	int bottom = g_h - STATUS_H;
	ui_canvas cv = files_canvas();
	ui_fill(cv, 0, top, SIDEBAR_W - 1, bottom - top, g_t.surface);
	ui_fill(cv, SIDEBAR_W - 1, top, 1, bottom - top, COL_DIVIDER);

	section_label(16, top + 8 + 12, "Places");
	int y = SB_ROW_Y0;
	for (int i = 0; i < g_nplaces && y + ROW_H <= bottom - 80; i++)
	{
		bool here = (strcmp(g_places[i].path, g_cwd) == 0);
		if (here)
			ui_fill_round(cv, 8, y, SIDEBAR_W - 17, ROW_H, UI_R_MD, g_t.accent_900);
		ui_draw_icon_tint(
		    cv, ui_icon_get(16, place_icon(&g_places[i])), 16, y + 6, here ? g_t.accent_400 : COL_TEXT_DIM);
		text_at(g_font, 16 + 16 + 8, y + ROW_H / 2, g_places[i].label, here ? g_t.accent_200 : COL_TEXT);
		y += ROW_H + 2;
	}

	/* Devices: the root pool with its usage. */
	struct ubix_pool_info pi[2];
	int np = ubix_pool_query(pi, 2);
	if (np > 0 && pi[0].total_blocks > 0)
	{
		y += 12;
		section_label(16, y + 12, "Devices");
		y += 24;
		uint64_t bs = pi[0].block_size ? pi[0].block_size : 4096;
		uint64_t total = pi[0].total_blocks * bs, used = (pi[0].total_blocks - pi[0].free_blocks) * bs;
		ui_draw_icon_tint(cv, ui_icon_get(16, "drive"), 16, y + 6, COL_TEXT_DIM);
		text_at(g_font, 40, y + ROW_H / 2, "System", COL_TEXT);
		char u[24], t[24], meta[56];
		hsize(used, u, sizeof(u));
		hsize(total, t, sizeof(t));
		snprintf(meta, sizeof(meta), "%s / %s", u, t);
		text_at(
		    g_font_xs, SIDEBAR_W - 16 - (int)g_font_xs.TextWidth(meta), y + ROW_H / 2, meta, g_t.text_faint);
		int bx = 40, bw = SIDEBAR_W - 16 - bx, by = y + ROW_H + 2;
		ui_fill_round(cv, bx, by, bw, 4, 2, g_t.neutral_800);
		int fw = (int)(used * (uint64_t)bw / total);
		if (fw > 0)
			ui_fill_round(cv, bx, by, fw < 4 ? 4 : fw, 4, 2, g_t.accent);
	}
}

/**
 * Index of the sidebar place at (x,y), or -1 if the point is outside the list.
 */
static int sidebar_hit(int x, int y)
{
	if (x >= SIDEBAR_W || y < SB_ROW_Y0)
		return -1;
	int i = (y - SB_ROW_Y0) / (ROW_H + 2);
	return (i >= 0 && i < g_nplaces) ? i : -1;
}

/* Toolbar control rectangles (set by draw_toolbar, used by on_click). */
static int g_crumb_x0, g_crumb_x1; /* breadcrumb field */
static int g_search_x, g_search_w; /* search field */
static int g_view_x;               /* view toggle (72 wide: grid | list) */

/**
 * Draw the toolbar (design files.html): Back / Forward / Up icon buttons, the
 * breadcrumb field, the search field and the grid/list view toggle.  Records
 * crumb spans in g_crumb for click navigation.
 */
static void draw_toolbar(void)
{
	ui_canvas cv = files_canvas();
	ui_fill(cv, 0, 0, g_w, TOOLBAR_H, COL_TOOLBAR);
	ui_fill(cv, 0, TOOLBAR_H - 1, g_w, 1, COL_DIVIDER);
	const int cy = TOOLBAR_H / 2, ch = 32, ty = (TOOLBAR_H - ch) / 2;

	bool en[BTN_COUNT] = {g_hist_pos > 0, g_hist_pos + 1 < g_hist_len, !(g_cwd[0] == '/' && g_cwd[1] == '\0')};
	int bx = 12;
	for (int i = 0; i < BTN_COUNT; i++)
	{
		g_btn[i].x = bx;
		g_btn[i].y = ty;
		g_btn[i].w = ch;
		g_btn[i].h = ch;
		g_btn[i].enabled = en[i];
		ui_draw_icon_tint(
		    cv, ui_icon_get(16, g_btn_icon[i]), bx + 8, ty + 8, en[i] ? COL_TEXT : g_t.text_faint);
		bx += ch + 4;
	}

	/* right side first: view toggle, then search */
	g_view_x = g_w - 12 - 72;
	g_search_w = 220;
	g_search_x = g_view_x - 8 - g_search_w;
	g_crumb_x0 = bx + 4;
	g_crumb_x1 = g_search_x - 8;

	/* breadcrumb field */
	ui_fill_round(cv, g_crumb_x0, ty, g_crumb_x1 - g_crumb_x0, ch, UI_R_MD, g_t.surface);
	g_ncrumb = 0;
	int x = g_crumb_x0 + 8;
	char acc[512] = "";
	const char *p = g_cwd;
	/* root crumb */
	g_crumb[0].x0 = x;
	text_at(g_font, x + 6, cy, "/", g_cwd[1] ? COL_TEXT_DIM : COL_TEXT);
	g_crumb[0].x1 = x + 12 + (int)g_font.TextWidth("/");
	snprintf(g_crumb[0].path, sizeof(g_crumb[0].path), "/");
	g_ncrumb = 1;
	x = g_crumb[0].x1;
	while (*p == '/')
		p++;
	char comp[NAME_MAX_LEN];
	while (*p && g_ncrumb < MAX_CRUMBS && x < g_crumb_x1 - 40)
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
		ui_draw_icon_tint(cv, ui_icon_get(16, "chevron-right"), x, cy - 8, g_t.text_faint);
		x += 16 + 2;
		bool cur = (*p == '\0');
		text_at(g_font, x + 6, cy, comp, cur ? COL_TEXT : COL_TEXT_DIM);
		g_crumb[g_ncrumb].x0 = x;
		g_crumb[g_ncrumb].x1 = x + 12 + (int)g_font.TextWidth(comp);
		snprintf(g_crumb[g_ncrumb].path, sizeof(g_crumb[0].path), "%s", acc);
		x = g_crumb[g_ncrumb].x1;
		g_ncrumb++;
	}

	/* search field */
	ui_outlined_round(
	    cv, g_search_x, ty, g_search_w, ch, UI_R_MD, g_search_focus ? g_t.accent : g_t.edge, g_t.ground);
	if (g_search_focus)
		ui_focus_ring(cv, g_search_x, ty, g_search_w, ch, UI_R_MD, g_t.accent);
	ui_draw_icon_tint(cv, ui_icon_get(16, "search"), g_search_x + 12, ty + 8, COL_TEXT_DIM);
	int sx = g_search_x + 12 + 16 + 8;
	if (g_query[0])
		text_at(g_font, sx, cy, g_query, COL_TEXT);
	else if (!g_search_focus)
	{
		const char *base = strrchr(g_cwd, '/');
		char ph[80];
		snprintf(ph, sizeof(ph), "Search %s", (base && base[1]) ? base + 1 : "/");
		text_at(g_font, sx, cy, ph, g_t.text_faint);
	}
	if (g_search_focus)
		ui_fill(cv, sx + (int)g_font.TextWidth(g_query) + 1, cy - 8, 1, 16, g_t.accent_400);

	/* view toggle (segmented: grid | list) */
	ui_fill_round(cv, g_view_x, ty, 72, ch, 16, g_t.surface);
	int half = (72 - 6 - 2) / 2;
	for (int v = 0; v < 2; v++)
	{
		int ox = g_view_x + 3 + v * (half + 2);
		bool sel = (g_view == v);
		if (sel)
			ui_outlined_round(cv, ox, ty + 3, half, ch - 6, (ch - 6) / 2, g_t.accent, g_t.accent_900);
		ui_draw_icon_tint(cv,
		                  ui_icon_get(16, v == VIEW_GRID ? "grid" : "list"),
		                  ox + (half - 16) / 2,
		                  ty + 8,
		                  sel ? g_t.accent_200 : COL_TEXT_DIM);
	}
}

/**
 * Draw one sortable column header label (list view), with an ascending /
 * descending caret on the active sort key.
 */
static void draw_head_label(int x, const char *label, int col)
{
	char buf[48];
	if (col == g_sort)
		snprintf(buf, sizeof(buf), "%s %s", label, g_sort_desc ? "v" : "^");
	else
		snprintf(buf, sizeof(buf), "%s", label);
	text_at(g_font_sm, x, content_top() + HEAD_H / 2, buf, col == g_sort ? COL_TEXT : COL_TEXT_DIM);
}

/**
 * Draw the column-header row (Name / Size / Type / Modified) — list view.
 */
static void draw_header(void)
{
	int hy = content_top();
	ui_canvas cv = files_canvas();
	ui_fill(cv, content_x(), hy, g_w - content_x(), HEAD_H, COL_WIN);
	ui_fill(cv, content_x() + PAD, hy + HEAD_H - 1, g_w - content_x() - 2 * PAD, 1, COL_DIVIDER);
	draw_head_label(name_x(), "Name", SORT_NAME);
	draw_head_label(col_size_r() - (int)g_font_sm.TextWidth("Size") - 14, "Size", SORT_SIZE);
	draw_head_label(col_type_x(), "Type", SORT_TYPE);
	draw_head_label(col_mod_x(), "Modified", SORT_MOD);
}

/**
 * Draw the visible slice of the list view: 28px rounded rows, the selection,
 * and the inline rename editor when active.
 */
static void draw_list(void)
{
	int top = list_top();
	int bottom = g_h - STATUS_H;
	int cx = content_x();
	ui_canvas cv = files_canvas();
	ui_fill(cv, cx, top, g_w - cx, bottom - top, COL_WIN);

	int rows = visible_rows();
	for (int r = 0; r < rows; r++)
	{
		int i = g_top + r;
		if (i >= g_nent)
			break;
		struct fentry *e = &g_ent[i];
		int y = top + r * ROW_H;
		int cy = y + ROW_H / 2;
		bool sel = (i == g_sel);

		if (sel)
			ui_fill_round(cv, cx + 8, y, g_w - cx - 16, ROW_H, UI_R_MD, COL_SEL);
		draw_icon(cx + PAD + 4, y + (ROW_H - 16) / 2, e->is_dir);

		uint32_t fg = sel ? COL_SEL_TEXT : COL_TEXT;
		uint32_t dim = sel ? COL_SEL_TEXT : COL_TEXT_DIM;

		if (g_mode == MODE_RENAME && sel)
		{
			int ex = name_x();
			ui_outlined_round(
			    cv, ex - 4, y + 3, col_size_r() - 16 - ex, ROW_H - 6, UI_R_SM, g_t.accent, g_t.ground);
			char shown[NAME_MAX_LEN + 2];
			snprintf(shown, sizeof(shown), "%s|", g_edit);
			text_at(g_font, ex, cy, shown, COL_TEXT);
		}
		else
			text_at(g_font, name_x(), cy, e->name, fg);

		char sz[24];
		if (e->is_dir)
			snprintf(sz, sizeof(sz), "--");
		else
			hsize(e->size, sz, sizeof(sz));
		text_at(g_font, col_size_r() - (int)g_font.TextWidth(sz) - 14, cy, sz, dim);

		char ty_s[40];
		type_of(e, ty_s, sizeof(ty_s));
		text_at(g_font, col_type_x(), cy, ty_s, dim);

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
				text_at(g_font, col_mod_x(), cy, mod, dim);
			}
		}
	}
}

/**
 * Split a tile label into at most two lines that fit width w (the second
 * line ellipsised).
 */
static void tile_lines(const char *name, int w, char *l1, size_t n1, char *l2, size_t n2)
{
	l1[0] = l2[0] = '\0';
	size_t len = strlen(name), cut = len;
	while (cut > 0)
	{
		char t[NAME_MAX_LEN];
		snprintf(t, sizeof(t), "%.*s", (int)cut, name);
		if ((int)g_font_sm.TextWidth(t) <= w)
			break;
		cut--;
	}
	snprintf(l1, n1, "%.*s", (int)cut, name);
	if (cut < len)
	{
		/* second line: the rest, trimmed with "..." if needed */
		const char *rest = name + cut;
		snprintf(l2, n2, "%s", rest);
		while (strlen(l2) > 1 && (int)g_font_sm.TextWidth(l2) > w)
		{
			size_t k = strlen(l2);
			l2[k - 1] = '\0';
			if (k >= 4)
				memcpy(l2 + k - 4, "...", 4);
		}
	}
}

/**
 * Draw the grid view: 112x100 tiles, a 48px file-type icon over a two-line
 * label; the selection in the accent tint with an accent-700 outline.
 */
static void draw_grid(void)
{
	int top = content_top();
	int bottom = g_h - STATUS_H;
	int cx = content_x();
	ui_canvas cv = files_canvas();
	ui_fill(cv, cx, top, g_w - cx, bottom - top, COL_WIN);

	int cols = grid_cols(), rows = grid_rows();
	for (int r = 0; r <= rows; r++)
		for (int c = 0; c < cols; c++)
		{
			int i = (g_top + r) * cols + c;
			if (i >= g_nent)
				return;
			int x = cx + GRID_PX + c * (TILE_W + TILE_GAP), y = top + GRID_PY + r * TILE_H;
			if (y + TILE_H > bottom)
				return;
			struct fentry *e = &g_ent[i];
			bool sel = (i == g_sel);
			if (sel)
				ui_outlined_round(cv, x, y, TILE_W, TILE_H, UI_R_MD, g_t.accent_700, g_t.accent_900);
			ui_draw_icon(cv, ui_icon_get(48, file_icon48(e)), x + (TILE_W - 48) / 2, y + 12);
			char l1[NAME_MAX_LEN], l2[NAME_MAX_LEN];
			tile_lines(e->name, TILE_W - 8, l1, sizeof(l1), l2, sizeof(l2));
			uint32_t fg = sel ? COL_SEL_TEXT : COL_TEXT;
			if (g_mode == MODE_RENAME && sel)
			{
				char shown[NAME_MAX_LEN + 2];
				snprintf(shown, sizeof(shown), "%s|", g_edit);
				ui_outlined_round(cv, x + 2, y + 66, TILE_W - 4, 20, UI_R_SM, g_t.accent, g_t.ground);
				ui_text_c(g_surf, g_font_sm, x + TILE_W / 2, y + 76, shown, COL_TEXT);
				continue;
			}
			ui_text_c(g_surf, g_font_sm, x + TILE_W / 2, y + 12 + 48 + 8 + 7, l1, fg);
			if (l2[0])
				ui_text_c(g_surf, g_font_sm, x + TILE_W / 2, y + 12 + 48 + 8 + 7 + 14, l2, fg);
		}
}

/**
 * Draw the status bar (design: 24px, 11px faint text): item count (or a
 * transient message), the selection with its size, free space on the right.
 */
static void draw_status(void)
{
	int sy = g_h - STATUS_H;
	ui_canvas cv = files_canvas();
	ui_fill(cv, 0, sy, g_w, STATUS_H, COL_TOOLBAR);
	ui_fill(cv, 0, sy, g_w, 1, COL_DIVIDER);
	int cy = sy + STATUS_H / 2;
	text_at(g_font_sm, 12, cy, g_status, g_t.text_faint);
	int x = 12 + (int)g_font_sm.TextWidth(g_status) + 24;

	if (g_sel >= 0 && g_sel < g_nent)
	{
		char right[NAME_MAX_LEN + 48];
		if (g_ent[g_sel].is_dir)
			snprintf(right, sizeof(right), "\"%s\" selected", g_ent[g_sel].name);
		else
		{
			char sz[24];
			hsize(g_ent[g_sel].size, sz, sizeof(sz));
			snprintf(right, sizeof(right), "\"%s\" selected \xB7 %s", g_ent[g_sel].name, sz);
		}
		text_at(g_font_sm, x, cy, right, g_t.text_faint);
	}

	struct ubix_pool_info pi[2];
	if (ubix_pool_query(pi, 2) > 0 && pi[0].total_blocks > 0)
	{
		char fs[24], msg[40];
		hsize(pi[0].free_blocks * (uint64_t)(pi[0].block_size ? pi[0].block_size : 4096), fs, sizeof(fs));
		snprintf(msg, sizeof(msg), "%s free", fs);
		text_at(g_font_sm, g_w - 16 - (int)g_font_sm.TextWidth(msg), cy, msg, g_t.text_faint);
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

	/* Did the click land on a list row? */
	if (mx >= content_x() && my >= list_top() && my < g_h - STATUS_H)
	{
		int r = (my - list_top()) / ROW_H;
		int i = g_top + r;
		if (i >= 0 && i < g_nent)
		{
			g_sel = i;
			g_ctx_target = i;
		}
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
	if (g_view == VIEW_GRID)
		draw_grid();
	else
	{
		draw_list();
		draw_header();
	}
	draw_sidebar();
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
	/* g_top counts list rows, or grid rows of tiles in the grid view */
	int line = (g_view == VIEW_GRID) ? g_sel / grid_cols() : g_sel;
	int rows = (g_view == VIEW_GRID) ? grid_rows() : visible_rows();
	if (line < g_top)
		g_top = line;
	else if (line >= g_top + rows)
		g_top = line - rows + 1;
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

	/* Toolbar: navigation buttons, breadcrumb, search, view toggle. */
	if (y < TOOLBAR_H)
	{
		bool was_search = g_search_focus;
		g_search_focus = false;
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
		else if (x >= g_search_x && x < g_search_x + g_search_w)
			g_search_focus = true;
		else if (x >= g_view_x && x < g_view_x + 72)
		{
			int v = (x < g_view_x + 36) ? VIEW_GRID : VIEW_LIST;
			if (v != g_view)
			{
				g_view = v;
				g_top = 0;
			}
		}
		else if (x >= g_crumb_x0 && x < g_crumb_x1)
		{
			for (int i = 0; i < g_ncrumb; i++)
				if (x >= g_crumb[i].x0 && x < g_crumb[i].x1)
				{
					navigate(g_crumb[i].path, true);
					break;
				}
		}
		(void)was_search;
		render();
		return;
	}
	g_search_focus = false;

	/* Sidebar place → jump there. */
	if (x < SIDEBAR_W)
	{
		int p = sidebar_hit(x, y);
		if (p >= 0)
			navigate(g_places[p].path, true);
		render();
		return;
	}

	/* List view: column header → sort. */
	if (g_view == VIEW_LIST && y >= content_top() && y < list_top())
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

	/* Content: a grid tile or a list row (double-click opens). */
	if (y >= content_top() && y < g_h - STATUS_H)
	{
		int i = -1;
		if (g_view == VIEW_GRID)
			i = grid_hit(x, y);
		else if (y >= list_top())
		{
			i = g_top + (y - list_top()) / ROW_H;
			if (i >= g_nent)
				i = -1;
		}
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
 * Handle a key press.  Routes to the rename editor or delete dialog when modal;
 * otherwise drives selection, navigation, type-ahead, and the action shortcuts.
 */
static void on_key(uint32_t kc)
{
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

	/* Search field: typing filters the current folder live. */
	if (g_search_focus)
	{
		size_t ql = strlen(g_query);
		if (kc == KEY_ESC)
		{
			if (ql)
				g_query[0] = '\0';
			else
				g_search_focus = false;
			read_dir();
		}
		else if (kc == '\r' || kc == '\n')
			g_search_focus = false;
		else if (kc == '\b' || kc == 0x7F)
		{
			if (ql)
				g_query[ql - 1] = '\0';
			read_dir();
		}
		else if (kc >= 0x20 && kc < 0x7F && ql < sizeof(g_query) - 1)
		{
			g_query[ql] = (char)kc;
			g_query[ql + 1] = '\0';
			read_dir();
		}
		else if (kc != KEY_UP && kc != KEY_DOWN && kc != KEY_LEFT && kc != KEY_RIGHT)
		{
			render();
			return;
		}
		else
			goto browse; /* arrows still move the selection */
		render();
		return;
	}

browse:
	/* Grid view: arrows move by tile / by row. */
	if (g_view == VIEW_GRID && (kc == KEY_LEFT || kc == KEY_RIGHT || kc == KEY_UP || kc == KEY_DOWN))
	{
		int step = (kc == KEY_LEFT) ? -1 : (kc == KEY_RIGHT) ? 1 : (kc == KEY_UP) ? -grid_cols() : grid_cols();
		if (g_sel < 0)
			g_sel = 0;
		else if (g_sel + step >= 0 && g_sel + step < g_nent)
			g_sel += step;
		ensure_visible();
		render();
		return;
	}
	if (kc == '/')
	{
		g_search_focus = true; /* "/" jumps to the search field */
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
	g_t = ui_theme_load(getenv("USER"));
	ui_style_buttons(g_t);
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
	g_font_sm.Load(FONT_PATH, 13);   /* 11px */
	g_font_xs.Load(FONT_PATH, 12);   /* 10px */
	if (!g_font.Load(FONT_PATH, 14)) /* 12px */
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
				struct display_mouse_ev *me = (struct display_mouse_ev *)ev.data;
				if (me->buttons & 2) /* right button → context menu */
					on_right_click(me->x, me->y);
				else if (me->buttons & 1)
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
