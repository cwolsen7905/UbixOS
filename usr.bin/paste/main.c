/*
 * paste — merge corresponding lines of files.
 *
 *   paste [-s] [-d LIST] [FILE...]
 *
 * Parallel mode (default) joins line N of every FILE, separated by the
 * delimiter list (default TAB, cycled between columns).  -s pastes serially:
 * all lines of one FILE are joined onto one output line.  "-" (or no FILE)
 * reads standard input.
 *
 * (C) 2002-2026 The UbixOS Project
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/**
 * Expand the backslash escapes (\n \t \0 \\) of a -d list.
 *
 * @param out  Buffer of at least strlen(in) bytes.
 * @return Number of delimiter characters written to @p out.
 */
static size_t expand_delims(const char *in, char *out)
{
	size_t n = 0;

	for (; *in; in++) {
		if (*in == '\\' && in[1]) {
			in++;
			switch (*in) {
			case 'n': out[n++] = '\n'; break;
			case 't': out[n++] = '\t'; break;
			case '0': out[n++] = '\0'; break;
			default:  out[n++] = *in; break;
			}
		} else {
			out[n++] = *in;
		}
	}
	return n;
}

/** Emit delimiter @p idx of the cycled list (a NUL entry means "none"). */
static void put_delim(const char *d, size_t nd, size_t idx)
{
	char c = d[idx % nd];

	if (c)
		putchar(c);
}

/** Strip the trailing newline (if any) from a getline() result. */
static void chomp(char *s, ssize_t len)
{
	if (len > 0 && s[len - 1] == '\n')
		s[len - 1] = '\0';
}

/**
 * Serial mode: join every line of @p fp onto one output line.
 */
static void paste_serial(FILE *fp, const char *d, size_t nd)
{
	char *line = NULL;
	size_t cap = 0, col = 0;
	ssize_t len;

	while ((len = getline(&line, &cap, fp)) >= 0) {
		if (col)
			put_delim(d, nd, col - 1);
		chomp(line, len);
		fputs(line, stdout);
		col++;
	}
	if (col)
		putchar('\n');
	free(line);
}

/**
 * Parallel mode: join line N of every stream until all are exhausted.
 */
static void paste_parallel(FILE **fp, int nfiles, const char *d, size_t nd)
{
	char *line = NULL;
	size_t cap = 0;

	for (;;) {
		char **row = calloc(nfiles, sizeof(*row));
		int alive = 0;

		if (!row)
			return;
		for (int f = 0; f < nfiles; f++) {
			ssize_t len;

			if (!fp[f])
				continue;
			len = getline(&line, &cap, fp[f]);
			if (len < 0) {
				if (fp[f] != stdin)
					fclose(fp[f]);
				fp[f] = NULL;
				continue;
			}
			chomp(line, len);
			row[f] = strdup(line);
			alive = 1;
		}
		if (alive) {
			for (int f = 0; f < nfiles; f++) {
				if (f)
					put_delim(d, nd, f - 1);
				if (row[f])
					fputs(row[f], stdout);
			}
			putchar('\n');
		}
		for (int f = 0; f < nfiles; f++)
			free(row[f]);
		free(row);
		if (!alive)
			break;
	}
	free(line);
}

int main(int argc, char **argv)
{
	int serial = 0, nfiles, i, rc = 0;
	const char *dlist = "\t";
	char *delims;
	size_t nd;
	FILE **fp;

	for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
		if (!strcmp(argv[i], "--")) {
			i++;
			break;
		}
		if (!strncmp(argv[i], "-d", 2)) {
			if (argv[i][2])
				dlist = argv[i] + 2;
			else if (i + 1 < argc)
				dlist = argv[++i];
			else {
				fprintf(stderr, "paste: -d needs an argument\n");
				return 1;
			}
		} else if (!strcmp(argv[i], "-s")) {
			serial = 1;
		} else {
			fprintf(stderr, "usage: paste [-s] [-d LIST] [FILE...]\n");
			return 1;
		}
	}

	delims = malloc(strlen(dlist) + 1);
	if (!delims)
		return 1;
	nd = expand_delims(dlist, delims);
	if (nd == 0)
		delims[nd++] = '\0';

	nfiles = argc - i;
	if (nfiles == 0) {
		static char *dash[] = { "-", NULL };

		argv = dash;
		i = 0;
		nfiles = 1;
	}

	fp = calloc(nfiles, sizeof(*fp));
	if (!fp)
		return 1;
	for (int f = 0; f < nfiles; f++) {
		const char *name = argv[i + f];

		if (!strcmp(name, "-")) {
			fp[f] = stdin;
		} else if (!(fp[f] = fopen(name, "r"))) {
			fprintf(stderr, "paste: %s: %s\n", name, strerror(errno));
			rc = 1;
		}
	}

	if (serial) {
		for (int f = 0; f < nfiles; f++)
			if (fp[f])
				paste_serial(fp[f], delims, nd);
	} else {
		paste_parallel(fp, nfiles, delims, nd);
	}
	return rc;
}
