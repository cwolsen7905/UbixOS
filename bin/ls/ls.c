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
 * ls — list directory contents.  A self-contained POSIX implementation written
 * against libc only (opendir/readdir/lstat) — no fts(3) (musl ships none) and no
 * busybox/libbb scaffolding, so it is fully debuggable on uBixOS' VFS.
 *
 * Flags: -l (long) -a (all) -A (almost-all) -1 (one per line) -d (dir as file)
 *        -F (type suffix) -i (inode) -R (recursive) -t (sort mtime) -S (sort
 *        size) -r (reverse) -h (human size) -n (numeric uid/gid).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <errno.h>

/* ── options ────────────────────────────────────────────────────────────────*/
static int g_opt_long;    /* -l */
static int g_opt_all;     /* -a : include . and .. */
static int g_opt_almost;  /* -A : include dotfiles but not . / .. */
static int g_opt_one;     /* -1 : one entry per line */
static int g_opt_dir;     /* -d : list directories as plain entries */
static int g_opt_class;   /* -F : append a type-indicator character */
static int g_opt_inode;   /* -i : print the inode number */
static int g_opt_recurse; /* -R : recurse into subdirectories */
static int g_opt_time;    /* -t : sort by modification time */
static int g_opt_size;    /* -S : sort by size (largest first) */
static int g_opt_rev;     /* -r : reverse the sort order */
static int g_opt_human;   /* -h : human-readable sizes */
static int g_opt_numeric; /* -n : numeric uid/gid (also implies -l) */

static int g_termwidth = 80;
static int g_exit = 0; /* accumulated exit status */

/* One listed entry: its name plus its lstat (taken once, reused everywhere). */
struct entry
{
	char *name;
	struct stat st;
	int statok;
};

/* ── helpers ────────────────────────────────────────────────────────────────*/

/**
 * Render a mode word as the 10-char "drwxr-xr-x" string into @out (>=11 bytes).
 */
static void mode_string(mode_t m, char *out)
{
	if (S_ISDIR(m))
		out[0] = 'd';
	else if (S_ISLNK(m))
		out[0] = 'l';
	else if (S_ISCHR(m))
		out[0] = 'c';
	else if (S_ISBLK(m))
		out[0] = 'b';
	else if (S_ISFIFO(m))
		out[0] = 'p';
	else if (S_ISSOCK(m))
		out[0] = 's';
	else
		out[0] = '-';

	out[1] = (m & S_IRUSR) ? 'r' : '-';
	out[2] = (m & S_IWUSR) ? 'w' : '-';
	out[3] = (m & S_IXUSR) ? 'x' : '-';
	out[4] = (m & S_IRGRP) ? 'r' : '-';
	out[5] = (m & S_IWGRP) ? 'w' : '-';
	out[6] = (m & S_IXGRP) ? 'x' : '-';
	out[7] = (m & S_IROTH) ? 'r' : '-';
	out[8] = (m & S_IWOTH) ? 'w' : '-';
	out[9] = (m & S_IXOTH) ? 'x' : '-';
	out[10] = '\0';
}

/**
 * The -F / classification suffix character for a mode, or '\0' for none.
 */
static char type_suffix(mode_t m)
{
	if (S_ISDIR(m))
		return ('/');
	if (S_ISLNK(m))
		return ('@');
	if (S_ISFIFO(m))
		return ('|');
	if (S_ISSOCK(m))
		return ('=');
	if ((m & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0)
		return ('*');
	return ('\0');
}

/**
 * Format @bytes as a short human-readable size ("4.0K", "1.7G") into @out.
 */
static void human_size(off_t bytes, char *out, size_t outsz)
{
	static const char unit[] = "BKMGTP";
	double v = (double)bytes;
	int i = 0;

	while (v >= 1024.0 && i < 5)
	{
		v /= 1024.0;
		i++;
	}
	if (i == 0)
		snprintf(out, outsz, "%ld", (long)bytes);
	else if (v < 10.0)
		snprintf(out, outsz, "%.1f%c", v, unit[i]);
	else
		snprintf(out, outsz, "%.0f%c", v, unit[i]);
}

/**
 * Resolve a uid to a user name, or its decimal form when no name maps (uBixOS
 * has /etc/userdb, not /etc/passwd, so getpwuid usually returns NULL).
 */
static void uid_name(uid_t uid, char *out, size_t outsz)
{
	struct passwd *pw = g_opt_numeric ? NULL : getpwuid(uid);
	if (pw != NULL && pw->pw_name != NULL)
		snprintf(out, outsz, "%s", pw->pw_name);
	else
		snprintf(out, outsz, "%u", (unsigned)uid);
}

static void gid_name(gid_t gid, char *out, size_t outsz)
{
	struct group *gr = g_opt_numeric ? NULL : getgrgid(gid);
	if (gr != NULL && gr->gr_name != NULL)
		snprintf(out, outsz, "%s", gr->gr_name);
	else
		snprintf(out, outsz, "%u", (unsigned)gid);
}

/* ── sorting ────────────────────────────────────────────────────────────────*/

static int entry_cmp(const void *a, const void *b)
{
	const struct entry *ea = (const struct entry *)a;
	const struct entry *eb = (const struct entry *)b;
	int r;

	if (g_opt_time && ea->statok && eb->statok)
	{
		if (ea->st.st_mtim.tv_sec != eb->st.st_mtim.tv_sec)
			r = (eb->st.st_mtim.tv_sec > ea->st.st_mtim.tv_sec) ? 1 : -1; /* newest first */
		else
			r = strcmp(ea->name, eb->name);
	}
	else if (g_opt_size && ea->statok && eb->statok)
	{
		if (ea->st.st_size != eb->st.st_size)
			r = (eb->st.st_size > ea->st.st_size) ? 1 : -1; /* largest first */
		else
			r = strcmp(ea->name, eb->name);
	}
	else
	{
		r = strcmp(ea->name, eb->name);
	}
	return (g_opt_rev ? -r : r);
}

/* ── output ─────────────────────────────────────────────────────────────────*/

/**
 * Print one entry in -l (long) form.
 */
static void print_long(const struct entry *e, const char *dirpath)
{
	char modes[11];
	char owner[32], grp[32], sizebuf[24], timebuf[32];
	struct tm *tm;
	const char *fmt;

	if (!e->statok)
	{
		printf("?????????? ? ? ? ? ? %s\n", e->name);
		return;
	}

	mode_string(e->st.st_mode, modes);
	uid_name(e->st.st_uid, owner, sizeof(owner));
	gid_name(e->st.st_gid, grp, sizeof(grp));
	if (g_opt_human)
		human_size(e->st.st_size, sizebuf, sizeof(sizebuf));
	else
		snprintf(sizebuf, sizeof(sizebuf), "%ld", (long)e->st.st_size);

	/* "Mon DD HH:MM" for recent files, "Mon DD  YYYY" for older — like BSD ls. */
	tm = localtime(&e->st.st_mtim.tv_sec);
	if (tm != NULL)
	{
		time_t now = time(NULL);
		long age = (long)(now - e->st.st_mtim.tv_sec);
		fmt = (age > 15552000L || age < -900L) ? "%b %e  %Y" : "%b %e %H:%M";
		strftime(timebuf, sizeof(timebuf), fmt, tm);
	}
	else
	{
		snprintf(timebuf, sizeof(timebuf), "?");
	}

	if (g_opt_inode)
		printf("%8lu ", (unsigned long)e->st.st_ino);
	printf("%s %3u %-8s %-8s %8s %s %s", modes, (unsigned)e->st.st_nlink, owner, grp, sizebuf, timebuf, e->name);

	/* Symlink target. */
	if (S_ISLNK(e->st.st_mode))
	{
		char path[1024], target[1024];
		ssize_t n;
		if (dirpath != NULL && strcmp(dirpath, ".") != 0)
			snprintf(path, sizeof(path), "%s/%s", dirpath, e->name);
		else
			snprintf(path, sizeof(path), "%s", e->name);
		n = readlink(path, target, sizeof(target) - 1);
		if (n > 0)
		{
			target[n] = '\0';
			printf(" -> %s", target);
		}
	}
	if (g_opt_class)
	{
		char c = type_suffix(e->st.st_mode);
		if (c != '\0')
			putchar(c);
	}
	putchar('\n');
}

/**
 * Print one short-form name (inode + name + optional -F suffix), no newline.
 */
static void print_short_name(const struct entry *e)
{
	if (g_opt_inode && e->statok)
		printf("%8lu ", (unsigned long)e->st.st_ino);
	fputs(e->name, stdout);
	if (g_opt_class && e->statok)
	{
		char c = type_suffix(e->st.st_mode);
		if (c != '\0')
			putchar(c);
	}
}

/**
 * Visible width of a short-form entry (for column layout).
 */
static int entry_width(const struct entry *e)
{
	int w = (int)strlen(e->name);
	if (g_opt_inode)
		w += 9;
	if (g_opt_class && e->statok && type_suffix(e->st.st_mode) != '\0')
		w += 1;
	return (w);
}

/**
 * Lay the entries out in down-then-across columns sized to the terminal width
 * (the classic ls grid).  Falls back to one-per-line for -1 or a tty-less run.
 */
static void print_columns(struct entry *ents, int n)
{
	int i, maxw = 0, cols, rows, col, row;

	if (g_opt_one || n == 0)
	{
		for (i = 0; i < n; i++)
		{
			print_short_name(&ents[i]);
			putchar('\n');
		}
		return;
	}

	for (i = 0; i < n; i++)
	{
		int w = entry_width(&ents[i]);
		if (w > maxw)
			maxw = w;
	}
	maxw += 2; /* gutter */

	cols = g_termwidth / maxw;
	if (cols < 1)
		cols = 1;
	rows = (n + cols - 1) / cols;

	for (row = 0; row < rows; row++)
	{
		for (col = 0; col < cols; col++)
		{
			int idx = col * rows + row;
			if (idx >= n)
				continue;
			print_short_name(&ents[idx]);
			if (col + 1 < cols && (col + 1) * rows + row < n)
			{
				int pad = maxw - entry_width(&ents[idx]);
				while (pad-- > 0)
					putchar(' ');
			}
		}
		putchar('\n');
	}
}

/* ── directory listing ──────────────────────────────────────────────────────*/

static void list_dir(const char *path, int show_header);

/**
 * Build the lstat path for @name inside @dir (".": bare name).
 */
static void join_path(const char *dir, const char *name, char *out, size_t outsz)
{
	if (dir == NULL || dir[0] == '\0' || strcmp(dir, ".") == 0)
		snprintf(out, outsz, "%s", name);
	else if (dir[strlen(dir) - 1] == '/')
		snprintf(out, outsz, "%s%s", dir, name);
	else
		snprintf(out, outsz, "%s/%s", dir, name);
}

/**
 * Read, stat, sort and print the contents of directory @path.  Recurses into
 * subdirectories afterward when -R is set.
 */
static void list_dir(const char *path, int show_header)
{
	DIR *d;
	struct dirent *de;
	struct entry *ents = NULL;
	int n = 0, cap = 0, i;

	d = opendir(path);
	if (d == NULL)
	{
		fprintf(stderr, "ls: %s: %s\n", path, strerror(errno));
		g_exit = 1;
		return;
	}

	while ((de = readdir(d)) != NULL)
	{
		char full[1024];

		if (de->d_name[0] == '.')
		{
			if (!g_opt_all && !g_opt_almost)
				continue;
			if (g_opt_almost && (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0))
				continue;
		}

		if (n == cap)
		{
			cap = cap ? cap * 2 : 64;
			ents = (struct entry *)realloc(ents, (size_t)cap * sizeof(*ents));
			if (ents == NULL)
			{
				fprintf(stderr, "ls: out of memory\n");
				closedir(d);
				exit(1);
			}
		}
		ents[n].name = strdup(de->d_name);
		join_path(path, de->d_name, full, sizeof(full));
		ents[n].statok = (lstat(full, &ents[n].st) == 0);
		n++;
	}
	closedir(d);

	qsort(ents, (size_t)n, sizeof(*ents), entry_cmp);

	if (show_header)
		printf("%s:\n", path);

	if (g_opt_long)
		for (i = 0; i < n; i++)
			print_long(&ents[i], path);
	else
		print_columns(ents, n);

	/* -R: recurse into real subdirectories (skip . and ..). */
	if (g_opt_recurse)
	{
		for (i = 0; i < n; i++)
		{
			char full[1024];
			if (!ents[i].statok || !S_ISDIR(ents[i].st.st_mode))
				continue;
			if (strcmp(ents[i].name, ".") == 0 || strcmp(ents[i].name, "..") == 0)
				continue;
			join_path(path, ents[i].name, full, sizeof(full));
			putchar('\n');
			list_dir(full, 1);
		}
	}

	for (i = 0; i < n; i++)
		free(ents[i].name);
	free(ents);
}

/* ── operands ───────────────────────────────────────────────────────────────*/

/**
 * List the explicit file operands (non-directories, or directories under -d),
 * one batch sorted together — the BSD ls behaviour of printing files before
 * directories.
 */
static void list_files(struct entry *files, int n)
{
	int i;

	qsort(files, (size_t)n, sizeof(*files), entry_cmp);
	if (g_opt_long)
		for (i = 0; i < n; i++)
			print_long(&files[i], ".");
	else
		print_columns(files, n);
}

static void usage(void)
{
	fprintf(stderr, "usage: ls [-aAdFhilnRrStl1] [file ...]\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int c, i;
	struct winsize ws;
	struct entry *files = NULL;
	int nfiles = 0;
	char **dirs = NULL;
	int ndirs = 0;
	int nops;

	while ((c = getopt(argc, argv, "aAdFhilnRrSt1")) != -1)
	{
		switch (c)
		{
			case 'a':
				g_opt_all = 1;
				break;
			case 'A':
				g_opt_almost = 1;
				break;
			case 'd':
				g_opt_dir = 1;
				break;
			case 'F':
				g_opt_class = 1;
				break;
			case 'h':
				g_opt_human = 1;
				break;
			case 'i':
				g_opt_inode = 1;
				break;
			case 'l':
				g_opt_long = 1;
				break;
			case 'n':
				g_opt_numeric = 1;
				g_opt_long = 1;
				break;
			case 'R':
				g_opt_recurse = 1;
				break;
			case 'r':
				g_opt_rev = 1;
				break;
			case 'S':
				g_opt_size = 1;
				break;
			case 't':
				g_opt_time = 1;
				break;
			case '1':
				g_opt_one = 1;
				break;
			default:
				usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* Column width from the terminal, else the 80-col default. */
	if (isatty(STDOUT_FILENO) && ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
		g_termwidth = ws.ws_col;
	else
		g_opt_one = g_opt_one || !isatty(STDOUT_FILENO);

	nops = argc;
	if (nops == 0)
	{
		/* No operands: list the current directory. */
		list_dir(".", 0);
		return (g_exit);
	}

	/* Split operands into plain files and directories (unless -d). */
	files = (struct entry *)calloc((size_t)nops, sizeof(*files));
	dirs = (char **)calloc((size_t)nops, sizeof(*dirs));
	if (files == NULL || dirs == NULL)
	{
		fprintf(stderr, "ls: out of memory\n");
		return (1);
	}

	for (i = 0; i < nops; i++)
	{
		struct stat st;
		if (lstat(argv[i], &st) != 0)
		{
			fprintf(stderr, "ls: %s: %s\n", argv[i], strerror(errno));
			g_exit = 1;
			continue;
		}
		if (S_ISDIR(st.st_mode) && !g_opt_dir)
		{
			dirs[ndirs++] = argv[i];
		}
		else
		{
			files[nfiles].name = argv[i];
			files[nfiles].st = st;
			files[nfiles].statok = 1;
			nfiles++;
		}
	}

	if (nfiles > 0)
		list_files(files, nfiles);

	for (i = 0; i < ndirs; i++)
	{
		/* Header when more than one thing is being listed. */
		if (nfiles > 0 || ndirs > 1 || g_opt_recurse)
		{
			if (i > 0 || nfiles > 0)
				putchar('\n');
			printf("%s:\n", dirs[i]);
		}
		list_dir(dirs[i], 0);
	}

	free(files);
	free(dirs);
	return (g_exit);
}
