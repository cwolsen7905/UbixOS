/*
 * libbb stub implementations for the UbixOS port of busybox vi.
 * Provides the minimum support functions vi.c imports from busybox's
 * libbb.  Kept tiny on purpose — anything fancier than malloc-or-die
 * gets revisited only if vi grows a dependency on it.
 */
#include "libbb.h"
#include "xregex.h"
#include <dirent.h>

int xfunc_error_retval = 1;
unsigned option_mask32;

/* The active applet sets applet_name in its own main() wrapper. */
const char *applet_name = "busybox";
const char bb_msg_standard_input[] = "standard input";
const char bb_msg_read_error[]     = "read error";
const char bb_msg_invalid_arg_to[] = "invalid argument '%s' to '%s'";
const char bb_msg_requires_arg[]   = "%s requires an argument";
long bb_arg_max                    = 131072;

char bb_common_bufsiz1[COMMON_BUFSIZE];

void setup_common_bufsiz(void)
{
	/* upstream zeros the scratch buffer here; we already have a BSS global */
}

/* Suffix table used by head/tail/dd-style "-n 100k" parsing. */
const struct suffix_mult bkm_suffixes[] = {
	{ "b",  512 },
	{ "k",  1024 },
	{ "K",  1024 },
	{ "m",  1024 * 1024 },
	{ "M",  1024 * 1024 },
	{ "",   0 }
};

unsigned long long xatoul_sfx(const char *numstr, const struct suffix_mult *suffixes)
{
	char *end;
	unsigned long long v;

	errno = 0;
	v = strtoull(numstr, &end, 10);
	if (errno || end == numstr)
		bb_simple_error_msg_and_die(numstr);
	if (*end != '\0' && suffixes) {
		const struct suffix_mult *s;
		for (s = suffixes; s->mult; s++) {
			if (strcmp(end, s->suffix) == 0) {
				v *= s->mult;
				end = (char *)"";
				break;
			}
		}
		if (*end != '\0')
			bb_simple_error_msg_and_die(numstr);
	}
	return v;
}

void die_if_ferror_stdout(void)
{
	if (ferror(stdout)) {
		bb_simple_error_msg_and_die("write error");
	}
}

void die_if_ferror(FILE *fp, const char *fn)
{
	if (ferror(fp)) {
		bb_error_msg_and_die("%s: %s", fn, "I/O error");
	}
}

unsigned xatou_sfx(const char *numstr, const struct suffix_mult *suffixes)
{
	unsigned long long v = xatoul_sfx(numstr, suffixes);
	if (v > UINT_MAX)
		bb_simple_error_msg_and_die(numstr);
	return (unsigned)v;
}

int xatoi(const char *numstr)
{
	char *end;
	long v;
	errno = 0;
	v = strtol(numstr, &end, 10);
	if (errno || end == numstr || *end != '\0' || v < INT_MIN || v > INT_MAX)
		bb_error_msg_and_die("invalid integer: %s", numstr);
	return (int)v;
}

int xatoi_positive(const char *numstr)
{
	int v = xatoi(numstr);
	if (v < 0)
		bb_error_msg_and_die("invalid positive integer: %s", numstr);
	return v;
}

unsigned long xatoul(const char *numstr)
{
	char *end;
	unsigned long v;
	errno = 0;
	v = strtoul(numstr, &end, 10);
	if (errno || end == numstr || *end != '\0')
		bb_error_msg_and_die("invalid number: %s", numstr);
	return v;
}

void xstat(const char *fileName, struct stat *statbuf)
{
	if (stat(fileName, statbuf) < 0)
		bb_perror_msg_and_die("can't stat '%s'", fileName);
}

int fdprintf(int fd, const char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	int n;
	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n < 0)
		return n;
	if ((size_t)n >= sizeof(buf))
		n = sizeof(buf) - 1;
	return (int)full_write(fd, buf, (size_t)n);
}

int open_or_warn_stdin(const char *filename)
{
	int fd;
	if (filename[0] == '-' && filename[1] == '\0')
		return 0;
	fd = open(filename, O_RDONLY);
	if (fd < 0)
		bb_simple_perror_msg(filename);
	return fd;
}

off_t xlseek(int fd, off_t offset, int whence)
{
	off_t r = lseek(fd, offset, whence);
	if (r == (off_t)-1)
		bb_perror_msg("lseek"), exit(1);
	return r;
}

void xwrite(int fd, const void *buf, size_t count)
{
	if ((size_t)full_write(fd, buf, count) != count)
		bb_perror_msg("write"), exit(1);
}

void xclose(int fd)
{
	if (close(fd) < 0)
		bb_perror_msg_and_die("close");
}

/* safe_strncpy: provided by contrib/busybox/libbb/safe_strncpy.c for tools
 * that pull it in.  Define a weak fallback here so other tools that don't
 * include the upstream object still link. */
__attribute__((weak))
char *safe_strncpy(char *dst, const char *src, size_t size)
{
	if (size == 0)
		return dst;
	strncpy(dst, src, size - 1);
	dst[size - 1] = '\0';
	return dst;
}

off_t bb_copyfd_eof(int fd1, int fd2)
{
	return bb_copyfd_size(fd1, fd2, -1);
}

/* Yes/No prompt to stderr — read one char from stdin, accept "y" / "Y". */
int bb_ask_y_confirmation(void)
{
	char c = 0;
	if (read(STDIN_FILENO, &c, 1) <= 0)
		return 0;
	/* Drain to end of line. */
	while (c != '\n' && c != '\0') {
		char tmp;
		if (read(STDIN_FILENO, &tmp, 1) <= 0)
			break;
		if (tmp == '\n')
			break;
	}
	return (c == 'y' || c == 'Y');
}

/* concat path/filename allowing a leading slash inside filename. */
char *concat_subpath_file(const char *path, const char *filename)
{
	if (filename && filename[0] == '.' &&
	    (filename[1] == '\0' ||
	     (filename[1] == '.' && filename[2] == '\0')))
		return NULL;
	return concat_path_file(path, filename);
}

/* ---------------------- grep / find / less helpers --------------------- */

FILE *fopen_for_read(const char *filename)
{
	return fopen(filename, "r");
}

FILE *fopen_for_write(const char *filename)
{
	return fopen(filename, "w");
}

FILE *xfopen_stdin(const char *filename)
{
	FILE *fp = fopen_or_warn_stdin(filename);
	if (!fp)
		exit(xfunc_error_retval);
	return fp;
}

/* Read a line into a freshly malloc'd buffer; strip trailing \n.
 * Returns NULL on EOF (no bytes read).  Differs from xmalloc_fgets in
 * that it always strips the newline. */
char *xmalloc_fgetline(FILE *fp)
{
	return xmalloc_fgets(fp);
}

void bb_error_msg_and_die(const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "%s: ", applet_name);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(xfunc_error_retval);
}

/* llist_add_to: push data onto the head of an llist_t pointed to by *old_head. */
void llist_add_to(llist_t **old_head, void *data)
{
	llist_t *node = xmalloc(sizeof(*node));
	node->data = data;
	node->link = *old_head;
	*old_head = node;
}

void llist_free(llist_t *elm, void (*freeit)(void *data))
{
	while (elm) {
		llist_t *next = elm->link;
		if (freeit)
			freeit(elm->data);
		free(elm);
		elm = next;
	}
}

/* xregcomp: regcomp() that dies with a helpful message on failure. */
char *regcomp_or_errmsg(regex_t *preg, const char *regex, int cflags)
{
	int err = regcomp(preg, regex, cflags);
	if (err == 0)
		return NULL;
	{
		size_t need = regerror(err, preg, NULL, 0);
		char *buf = xmalloc(need);
		regerror(err, preg, buf, need);
		return buf;
	}
}

void xregcomp(regex_t *preg, const char *regex, int cflags)
{
	char *msg = regcomp_or_errmsg(preg, regex, cflags);
	if (msg)
		bb_error_msg_and_die("bad regex '%s': %s", regex, msg);
}

/* ----------------------------- recursive_action -----------------------
 * Lightweight directory walker matching busybox's signature.  Calls
 * fileAction on every regular file/symlink and dirAction (if supplied)
 * on every directory entered.  Honours ACTION_RECURSE / ACTION_DEPTHFIRST.
 *
 * Returns 1 if every callback returned non-zero (success / "kept going"),
 * 0 if anything failed — matching upstream's contract closely enough for
 * grep -r, find's recursion, etc.
 */
static int recursive_walk(const char *path, unsigned flags, int depth,
                          recursive_action_fp fileAction,
                          recursive_action_fp dirAction,
                          void *userData)
{
	struct stat sb;
	int (*stat_fn)(const char *, struct stat *) = lstat;
	if (flags & ACTION_FOLLOWLINKS)
		stat_fn = stat;
	if (depth == 0 && (flags & ACTION_FOLLOWLINKS_L0))
		stat_fn = stat;

	if (stat_fn(path, &sb) < 0) {
		if (!(flags & ACTION_QUIET))
			bb_simple_perror_msg(path);
		return 0;
	}

	struct recursive_state state = { depth, (int)flags, userData };

	if (!S_ISDIR(sb.st_mode)) {
		if (fileAction)
			return fileAction(&state, path, &sb);
		return 1;
	}

	/* Directory */
	int ok = 1;
	if (!(flags & ACTION_DEPTHFIRST) && dirAction) {
		ok = dirAction(&state, path, &sb);
		if (!ok)
			return 0;
	}

	if (!(flags & ACTION_RECURSE) && depth > 0) {
		if ((flags & ACTION_DEPTHFIRST) && dirAction) {
			state.depth = depth;
			ok = dirAction(&state, path, &sb);
		}
		return ok;
	}

	DIR *d = opendir(path);
	if (!d) {
		if (!(flags & ACTION_QUIET))
			bb_simple_perror_msg(path);
		return 0;
	}
	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		if (ent->d_name[0] == '.' &&
		    (ent->d_name[1] == '\0' ||
		     (ent->d_name[1] == '.' && ent->d_name[2] == '\0')))
			continue;
		char *child = concat_path_file(path, ent->d_name);
		if (!recursive_walk(child, flags, depth + 1,
		                    fileAction, dirAction, userData))
			ok = 0;
		free(child);
	}
	closedir(d);

	if ((flags & ACTION_DEPTHFIRST) && dirAction) {
		struct recursive_state s = { depth, (int)flags, userData };
		if (!dirAction(&s, path, &sb))
			ok = 0;
	}
	return ok;
}

int recursive_action(const char *fileName, unsigned flags,
                     recursive_action_fp fileAction,
                     recursive_action_fp dirAction,
                     void *userData)
{
	return recursive_walk(fileName, flags, 0, fileAction, dirAction, userData);
}

/* ----------------------------- pager helpers --------------------------- */

int xopen(const char *pathname, int flags)
{
	int fd = open(pathname, flags);
	if (fd < 0) {
		bb_perror_msg("can't open '%s'", pathname);
		exit(xfunc_error_retval);
	}
	return fd;
}

char *xmalloc_ttyname(int fd)
{
	char buf[64];
	if (ttyname_r(fd, buf, sizeof(buf)) != 0)
		return NULL;
	return xstrdup(buf);
}

/* Ensure the vector has room for index `idx`.  Grows in chunks of
 * (1 << shift) entries when crossing a chunk boundary; busybox callers
 * assume each entry is sizeof(void*). */
void *xrealloc_vector(void *vector, unsigned shift, int idx)
{
	unsigned mask = (1U << shift) - 1;
	if ((unsigned)idx & mask)
		return vector;
	return xrealloc(vector, (size_t)(idx + (1 << shift)) * sizeof(void *));
}

/* Cat each argv entry (or stdin if none / "-") to stdout. */
int bb_cat(char **argv)
{
	int status = 0;
	if (!argv[0] || (argv[0][0] == '-' && argv[0][1] == '\0' && !argv[1])) {
		bb_copyfd_size(STDIN_FILENO, STDOUT_FILENO, -1);
		return 0;
	}
	for (; *argv; argv++) {
		int fd;
		if (argv[0][0] == '-' && argv[0][1] == '\0') {
			bb_copyfd_size(STDIN_FILENO, STDOUT_FILENO, -1);
			continue;
		}
		fd = open(*argv, O_RDONLY);
		if (fd < 0) {
			bb_simple_perror_msg(*argv);
			status = 1;
			continue;
		}
		bb_copyfd_size(fd, STDOUT_FILENO, -1);
		close(fd);
	}
	return status;
}

/* Install `handler` for each signal whose bit is set in `sigs`.  Iterate
 * a small fixed range — covers everything in BB_FATAL_SIGS. */
void bb_signals(unsigned sigs, void (*handler)(int))
{
	int s;
	for (s = 1; s < 32; s++) {
		if (sigs & (1U << s))
			signal(s, handler);
	}
}

int ndelay_on(int fd)
{
	int fl = fcntl(fd, F_GETFL);
	if (fl >= 0 && !(fl & O_NONBLOCK))
		fcntl(fd, F_SETFL, fl | O_NONBLOCK);
	return fl;
}

int ndelay_off(int fd)
{
	int fl = fcntl(fd, F_GETFL);
	if (fl >= 0 && (fl & O_NONBLOCK))
		fcntl(fd, F_SETFL, fl & ~O_NONBLOCK);
	return fl;
}

/* bb_basename: provided by contrib/busybox/libbb/get_last_path_component.c
 * (included in each applet that needs it).  Don't define a duplicate here. */

DIR *warn_opendir(const char *path)
{
	DIR *d = opendir(path);
	if (!d)
		bb_simple_perror_msg(path);
	return d;
}

unsigned get_terminal_width(int fd)
{
	unsigned w = 80;
	(void)get_terminal_width_height(fd, &w, NULL);
	return w;
}

/* Generate ls-style "mode string" like "-rwxr-xr-x" or "drwxrwxrwt". */
char *bb_mode_string(char buf[11], mode_t mode)
{
	static const char type_chars[16] = "?pc?d?b?-?l?s???";
	static const char mode_chars[7]  = "rwxSTst";

	buf[0]  = type_chars[(mode >> 12) & 0xf];
	buf[1]  = (mode & S_IRUSR) ? 'r' : '-';
	buf[2]  = (mode & S_IWUSR) ? 'w' : '-';
	if (mode & S_ISUID)
		buf[3] = (mode & S_IXUSR) ? 's' : 'S';
	else
		buf[3] = (mode & S_IXUSR) ? 'x' : '-';
	buf[4]  = (mode & S_IRGRP) ? 'r' : '-';
	buf[5]  = (mode & S_IWGRP) ? 'w' : '-';
	if (mode & S_ISGID)
		buf[6] = (mode & S_IXGRP) ? 's' : 'S';
	else
		buf[6] = (mode & S_IXGRP) ? 'x' : '-';
	buf[7]  = (mode & S_IROTH) ? 'r' : '-';
	buf[8]  = (mode & S_IWOTH) ? 'w' : '-';
	if (mode & S_ISVTX)
		buf[9] = (mode & S_IXOTH) ? 't' : 'T';
	else
		buf[9] = (mode & S_IXOTH) ? 'x' : '-';
	buf[10] = '\0';
	(void)mode_chars;
	return buf;
}

/* readlink into a freshly-malloc'd, NUL-terminated buffer.  Returns NULL
 * and leaves errno set on failure. */
char *xmalloc_readlink(const char *path)
{
	size_t cap = 256;
	for (;;) {
		char *buf = xmalloc(cap);
		ssize_t n = readlink(path, buf, cap - 1);
		if (n < 0) {
			free(buf);
			return NULL;
		}
		if ((size_t)n < cap - 1) {
			buf[n] = '\0';
			return buf;
		}
		free(buf);
		cap *= 2;
	}
}

/* Convert one char into a printable representation in buf[<=4 chars]:
 *  \xx for tabs (under VISIBLE_SHOW_TABS) or non-printables,
 *  ^X / M-^X for control / 8-bit, plain otherwise.  Appends terminator. */
void visible(unsigned ch, char *buf, int flags)
{
	if (ch == '\t' && !(flags & VISIBLE_SHOW_TABS)) {
		*buf++ = '\t';
		*buf = '\0';
		return;
	}
	if (ch == '\n') {
		if (flags & VISIBLE_ENDLINE)
			*buf++ = '$';
		*buf++ = '\n';
		*buf = '\0';
		return;
	}
	if (ch >= 128) {
		*buf++ = 'M';
		*buf++ = '-';
		ch -= 128;
	}
	if (ch < 32) {
		*buf++ = '^';
		*buf++ = (char)(ch + '@');
	} else if (ch == 127) {
		*buf++ = '^';
		*buf++ = '?';
	} else {
		*buf++ = (char)ch;
	}
	*buf = '\0';
}

int print_numbered_lines(struct number_state *ns, const char *filename)
{
	FILE *fp = fopen_or_warn_stdin(filename);
	unsigned N;
	char *line;

	if (!fp)
		return EXIT_FAILURE;

	N = ns->start;
	while ((line = xmalloc_fgetline(fp)) != NULL) {
		if (ns->all || (ns->nonempty && line[0])) {
			printf("%*u%s", ns->width, N, ns->sep);
			N += ns->inc;
		} else if (ns->empty_str) {
			fputs_stdout(ns->empty_str);
		}
		puts(line);
		free(line);
	}
	fclose_if_not_stdin(fp);
	return EXIT_SUCCESS;
}

char *xmalloc_readlink_or_warn(const char *path)
{
	char *buf = xmalloc_readlink(path);
	if (!buf) {
		const char *msg = (errno == EINVAL) ? "not a symlink" : strerror(errno);
		bb_error_msg("%s: %s", path, msg);
	}
	return buf;
}

/* Format a byte count with a human-readable suffix.  Returns a pointer
 * into a static buffer (overwritten by next call), matching busybox.
 * block_size: input is multiplied by this; display_unit picks the
 * suffix (0 = auto, 1 = bytes, 1024 = K, etc.). */
const char *make_human_readable_str(unsigned long long val,
                                    unsigned long block_size,
                                    unsigned long display_unit)
{
	static char buf[16];
	static const char fmt[] = " KMGTPE";
	unsigned int rem = 0;
	int suffix_idx = 0;

	val *= block_size;
	if (display_unit) {
		val += display_unit / 2;
		val /= display_unit;
	} else {
		while (val >= 1024) {
			rem = (unsigned int)(val % 1024);
			val /= 1024;
			suffix_idx++;
		}
	}
	if (suffix_idx == 0)
		snprintf(buf, sizeof(buf), "%llu", val);
	else if (val < 10) {
		unsigned int frac = (rem * 10 + 512) / 1024;
		if (frac == 10) { val++; frac = 0; }
		snprintf(buf, sizeof(buf), "%llu.%u%c", val, frac, fmt[suffix_idx]);
	} else {
		snprintf(buf, sizeof(buf), "%llu%c", val, fmt[suffix_idx]);
	}
	return buf;
}

void bb_perror_msg_and_die(const char *fmt, ...)
{
	va_list ap;
	int saved = errno;
	fprintf(stderr, "%s: ", applet_name);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, ": %s\n", strerror(saved));
	exit(xfunc_error_retval);
}

int get_termios_and_make_raw(int fd, struct termios *newterm,
                             struct termios *oldterm, int flags)
{
	if (tcgetattr(fd, oldterm) < 0)
		return -1;
	*newterm = *oldterm;
	newterm->c_iflag &= (tcflag_t)~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | IXON);
	if (flags & TERMIOS_RAW_CRNL)
		newterm->c_iflag |= ICRNL;
	else
		newterm->c_iflag &= (tcflag_t)~ICRNL;
	newterm->c_lflag &= (tcflag_t)~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	newterm->c_cflag &= (tcflag_t)~(CSIZE | PARENB);
	newterm->c_cflag |= CS8;
	newterm->c_cc[VMIN] = 1;
	newterm->c_cc[VTIME] = 0;
	return tcsetattr(fd, TCSANOW, newterm);
}

void kill_myself_with_sig(int sig)
{
	signal(sig, SIG_DFL);
	raise(sig);
	_exit(128 + sig);
}

/* vi.c uses `#define G (*ptr_to_globals)`; the storage lives here. */
struct globals *ptr_to_globals;

/* ------------------------- malloc-or-die family ------------------------- */

void *xmalloc(size_t size)
{
	void *p = malloc(size);
	if (!p) {
		fputs("out of memory\n", stderr);
		exit(1);
	}
	return p;
}

void *xrealloc(void *ptr, size_t size)
{
	void *p = realloc(ptr, size);
	if (!p && size) {
		fputs("out of memory\n", stderr);
		exit(1);
	}
	return p;
}

void *xzalloc(size_t size)
{
	void *p = xmalloc(size);
	memset(p, 0, size);
	return p;
}

char *xstrdup(const char *s)
{
	char *p = strdup(s);
	if (!p) {
		fputs("out of memory\n", stderr);
		exit(1);
	}
	return p;
}

char *xstrndup(const char *s, size_t n)
{
	size_t len = strnlen(s, n);
	char *p = xmalloc(len + 1);
	memcpy(p, s, len);
	p[len] = '\0';
	return p;
}

/* strchrnul: like strchr but returns the trailing NUL when c isn't found. */
char *strchrnul(const char *s, int c)
{
	while (*s && *s != (char)c)
		s++;
	return (char *)s;
}

/* memrchr: scan backwards for byte c in the first n bytes. */
void *memrchr(const void *s, int c, size_t n)
{
	const unsigned char *p = (const unsigned char *)s + n;
	while (n--) {
		if (*--p == (unsigned char)c)
			return (void *)p;
	}
	return NULL;
}

/* safe_poll: short busy-wait emulation of poll() over FIONREAD.  UbixOS's
 * TTY read in raw mode blocks per-byte and we don't ship a real poll(),
 * so we sleep in short slices and check for input each time.  Used only
 * by mysleep() / "press any key" in vi. */
int safe_poll(struct pollfd *ufds, nfds_t nfds, int timeout)
{
	int waited = 0;
	const int slice_ms = 20;

	for (;;) {
		nfds_t i;
		int ready = 0;
		for (i = 0; i < nfds; i++) {
			int n = 0;
			ufds[i].revents = 0;
			if (ioctl(ufds[i].fd, FIONREAD, &n) == 0 && n > 0) {
				ufds[i].revents = (short)(ufds[i].events & POLLIN);
				ready++;
			}
		}
		if (ready > 0)
			return ready;
		if (timeout >= 0 && waited >= timeout)
			return 0;
		usleep((useconds_t)slice_ms * 1000);
		waited += slice_ms;
	}
}

/* Read entire file into freshly malloc'd buffer.  If maxsz_p is non-NULL,
 * write the byte count there.  Returns NULL (and leaves errno) on failure. */
char *xmalloc_open_read_close(const char *filename, size_t *maxsz_p)
{
	int fd = open(filename, O_RDONLY);
	if (fd < 0)
		return NULL;
	struct stat st;
	if (fstat(fd, &st) < 0) {
		close(fd);
		return NULL;
	}
	size_t size = (size_t)st.st_size;
	char *buf = xmalloc(size + 1);
	ssize_t n = 0;
	size_t total = 0;
	while (total < size) {
		n = safe_read(fd, buf + total, size - total);
		if (n <= 0)
			break;
		total += (size_t)n;
	}
	close(fd);
	buf[total] = '\0';
	if (maxsz_p)
		*maxsz_p = total;
	return buf;
}

/* ----------------------------- I/O helpers ----------------------------- */

ssize_t safe_read(int fd, void *buf, size_t count)
{
	ssize_t n;
	do {
		n = read(fd, buf, count);
	} while (n < 0 && errno == EINTR);
	return n;
}

ssize_t full_read(int fd, void *buf, size_t count)
{
	char *p = buf;
	ssize_t total = 0;
	while (count > 0) {
		ssize_t n = safe_read(fd, p, count);
		if (n < 0)
			return n;
		if (n == 0)
			break;
		total += n;
		p += n;
		count -= (size_t)n;
	}
	return total;
}

ssize_t full_write(int fd, const void *buf, size_t count)
{
	const char *p = buf;
	ssize_t total = 0;
	while (count > 0) {
		ssize_t n = write(fd, p, count);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return n;
		}
		total += n;
		p += n;
		count -= (size_t)n;
	}
	return total;
}

void bb_putchar(int c)
{
	putchar(c);
}

int fputs_stdout(const char *s)
{
	return fputs(s, stdout);
}

int get_terminal_width_height(int fd, unsigned *width, unsigned *height)
{
	struct winsize ws;
	if (ioctl(fd, TIOCGWINSZ, &ws) < 0) {
		if (width)  *width  = 80;
		if (height) *height = 25;
		return -1;
	}
	if (width)  *width  = ws.ws_col ? ws.ws_col : 80;
	if (height) *height = ws.ws_row ? ws.ws_row : 25;
	return 0;
}

void fflush_all(void)
{
	fflush(NULL);
}

void bb_show_usage(void)
{
	fprintf(stderr, "%s: invalid usage\n", applet_name);
	exit(1);
}

void bb_simple_error_msg_and_die(const char *s)
{
	fprintf(stderr, "%s: %s\n", applet_name, s);
	exit(1);
}

void bb_simple_perror_msg(const char *s)
{
	fprintf(stderr, "%s: %s: %s\n", applet_name, s, strerror(errno));
}

void bb_simple_error_msg(const char *s)
{
	fprintf(stderr, "%s: %s\n", applet_name, s);
}

/* Returns pointer to the last char if it equals c, else NULL. */
char *last_char_is(const char *s, int c)
{
	if (!s || !*s)
		return NULL;
	{
		size_t n = strlen(s);
		return (s[n - 1] == (char)c) ? (char *)(s + n - 1) : NULL;
	}
}

int open_or_warn(const char *pathname, int flags)
{
	int fd = open(pathname, flags);
	if (fd < 0)
		bb_simple_perror_msg(pathname);
	return fd;
}

int open3_or_warn(const char *pathname, int flags, int mode)
{
	int fd = open(pathname, flags, mode);
	if (fd < 0)
		bb_simple_perror_msg(pathname);
	return fd;
}

/* Hardlink-preservation table stubs.  ENABLE_FEATURE_PRESERVE_HARDLINKS=0
 * in our config, so copy_file never asks; the stubs exist only so the
 * compile unit links. */
const char *is_in_ino_dev_hashtable(const struct stat *statbuf)
{
	(void)statbuf;
	return NULL;
}
void add_to_ino_dev_hashtable(const struct stat *statbuf, const char *name)
{
	(void)statbuf; (void)name;
}
void reset_ino_dev_hashtable(void)
{
}

/* Minimal POSIX dirname() — modifies path in place, returns ptr into it. */
char *dirname(char *path)
{
	char *slash;
	if (!path || !*path)
		return (char *)".";
	slash = strrchr(path, '/');
	if (!slash)
		return (char *)".";
	if (slash == path) {
		path[1] = '\0';
		return path;
	}
	*slash = '\0';
	return path;
}

FILE *fopen_or_warn_stdin(const char *filename)
{
	FILE *fp;
	if (filename[0] == '-' && filename[1] == '\0')
		return stdin;
	fp = fopen(filename, "r");
	if (!fp)
		bb_simple_perror_msg(filename);
	return fp;
}

FILE *fopen_or_warn(const char *path, const char *mode)
{
	FILE *fp = fopen(path, mode);
	if (!fp)
		bb_simple_perror_msg(path);
	return fp;
}

void bb_simple_perror_msg_and_die(const char *s)
{
	bb_simple_perror_msg(s);
	exit(xfunc_error_retval);
}

void bb_putchar_stderr(char ch)
{
	fputc(ch, stderr);
}

/* Read a line including the trailing newline into a freshly-malloc'd
 * buffer.  *end gets the terminating char (newline or EOF=0).  Returns
 * NULL on EOF with no bytes read; otherwise a NUL-terminated string. */
char *bb_get_chunk_from_file(FILE *file, int *end)
{
	size_t cap = 128;
	size_t len = 0;
	char *buf = xmalloc(cap);
	int c;
	int terminator = 0;

	for (;;) {
		c = getc(file);
		if (c == EOF) {
			if (len == 0) {
				free(buf);
				if (end) *end = 0;
				return NULL;
			}
			break;
		}
		if (len + 1 >= cap) {
			cap *= 2;
			buf = xrealloc(buf, cap);
		}
		buf[len++] = (char)c;
		if (c == '\n') {
			terminator = '\n';
			break;
		}
	}
	buf[len] = '\0';
	if (end) *end = terminator;
	return buf;
}

/* Decode one backslash escape (\n, \t, \xHH, \ooo, etc.) starting at
 * *ptr (which points one past the backslash).  Advances *ptr past the
 * consumed bytes and returns the decoded char.  Used by tr -d, tr -s. */
char bb_process_escape_sequence(const char **ptr)
{
	const char *p = *ptr;
	char c;

	switch (*p) {
	case 'a':  c = '\a'; p++; break;
	case 'b':  c = '\b'; p++; break;
	case 'f':  c = '\f'; p++; break;
	case 'n':  c = '\n'; p++; break;
	case 'r':  c = '\r'; p++; break;
	case 't':  c = '\t'; p++; break;
	case 'v':  c = '\v'; p++; break;
	case '\\': c = '\\'; p++; break;
	case '?':  c = '?';  p++; break;
	case '\'': c = '\''; p++; break;
	case '"':  c = '"';  p++; break;
	case 'x': {
		int n = 0; int v = 0;
		p++;
		while (n < 2 && isxdigit((unsigned char)*p)) {
			v = v * 16 + (isdigit((unsigned char)*p)
			              ? *p - '0'
			              : (*p | 0x20) - 'a' + 10);
			p++; n++;
		}
		c = (char)v;
		break;
	}
	default:
		if (*p >= '0' && *p <= '7') {
			int n = 0; int v = 0;
			while (n < 3 && *p >= '0' && *p <= '7') {
				v = v * 8 + (*p - '0');
				p++; n++;
			}
			c = (char)v;
		} else {
			c = *p;
			if (*p) p++;
		}
		break;
	}
	*ptr = p;
	return c;
}

int fclose_if_not_stdin(FILE *fp)
{
	if (fp == stdin)
		return 0;
	return fclose(fp);
}

void fflush_stdout_and_exit(int status)
{
	if (fflush(stdout) != 0)
		status = 1;
	exit(status);
}

/* xmalloc_fgets: read one line into a freshly-malloc'd buffer; strip the
 * trailing \n if any.  Returns NULL on EOF / error. */
char *xmalloc_fgets(FILE *fp)
{
	size_t cap = 128;
	size_t len = 0;
	char *buf = xmalloc(cap);
	int c;

	for (;;) {
		c = getc(fp);
		if (c == EOF) {
			if (len == 0) {
				free(buf);
				return NULL;
			}
			break;
		}
		if (len + 1 >= cap) {
			cap *= 2;
			buf = xrealloc(buf, cap);
		}
		if (c == '\n')
			break;
		buf[len++] = (char)c;
	}
	buf[len] = '\0';
	return buf;
}

void bb_error_msg(const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "%s: ", applet_name);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

void bb_perror_msg(const char *fmt, ...)
{
	va_list ap;
	int saved = errno;
	fprintf(stderr, "%s: ", applet_name);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, ": %s\n", strerror(saved));
}

void bb_perror_nomsg_and_die(void)
{
	fprintf(stderr, "%s: %s\n", applet_name, strerror(errno));
	exit(1);
}

void xmove_fd(int from, int to)
{
	if (from == to)
		return;
	if (dup2(from, to) < 0) {
		bb_perror_msg("dup2");
		exit(1);
	}
	close(from);
}

/* Copy up to `size` bytes from fd1 to fd2.  -1 size means until EOF.
 * Returns bytes actually copied. */
off_t bb_copyfd_size(int fd1, int fd2, off_t size)
{
	char buf[4096];
	off_t total = 0;

	while (size != 0) {
		size_t want = sizeof(buf);
		if (size > 0 && (off_t)want > size)
			want = (size_t)size;
		ssize_t n = safe_read(fd1, buf, want);
		if (n <= 0)
			break;
		if (full_write(fd2, buf, (size_t)n) != n)
			break;
		total += n;
		if (size > 0)
			size -= n;
	}
	return total;
}

unsigned bb_strtou(const char *arg, char **endp, int base)
{
	char *e;
	unsigned long v;
	errno = 0;
	v = strtoul(arg, &e, base);
	if (e == arg) {
		errno = EINVAL;
		return UINT_MAX;
	}
	if (endp)
		*endp = e;
	return (unsigned)v;
}

char *concat_path_file(const char *path, const char *filename)
{
	size_t lp;
	size_t lf;
	int sep;
	char *out;

	if (!path || !*path)
		return xstrdup(filename);
	lp = strlen(path);
	sep = (path[lp - 1] != '/');
	lf = strlen(filename);
	out = xmalloc(lp + (size_t)sep + lf + 1);
	memcpy(out, path, lp);
	if (sep)
		out[lp] = '/';
	memcpy(out + lp + (size_t)sep, filename, lf + 1);
	return out;
}

char *skip_whitespace(const char *s)
{
	while (*s == ' ' || *s == '\t')
		s++;
	return (char *)s;
}

char *skip_non_whitespace(const char *s)
{
	while (*s && *s != ' ' && *s != '\t')
		s++;
	return (char *)s;
}

void *llist_pop(llist_t **head)
{
	llist_t *node;
	void *data;

	if (!*head)
		return NULL;
	node = *head;
	data = node->data;
	*head = node->link;
	free(node);
	return data;
}

char *xasprintf(const char *fmt, ...)
{
	va_list ap;
	char *res = NULL;
	int n;

	va_start(ap, fmt);
	n = vasprintf(&res, fmt, ap);
	va_end(ap);
	if (n < 0 || !res) {
		fputs("out of memory\n", stderr);
		exit(1);
	}
	return res;
}

/* strings is a sequence of NUL-terminated C strings, terminated by an
 * empty string (i.e. "foo\0bar\0baz\0\0").  Returns the zero-based index
 * of the entry that equals key, or -1. */
int index_in_strings(const char *strings, const char *key)
{
	int i = 0;
	while (*strings) {
		if (strcmp(strings, key) == 0)
			return i;
		strings += strlen(strings) + 1;
		i++;
	}
	return -1;
}

/* ---------------------------- termios helpers ---------------------------- */

void tcsetattr_stdin_TCSANOW(const struct termios *tio)
{
	tcsetattr(STDIN_FILENO, TCSANOW, tio);
}

void set_termios_to_raw(int fd, struct termios *orig_out, int flags)
{
	struct termios t;

	if (tcgetattr(fd, &t) < 0)
		return;
	if (orig_out)
		*orig_out = t;

	t.c_iflag &= (tcflag_t)~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | IXON);
	if (flags & TERMIOS_RAW_CRNL)
		t.c_iflag |= ICRNL;
	else
		t.c_iflag &= (tcflag_t)~ICRNL;
	t.c_oflag &= (tcflag_t)~OPOST;
	t.c_lflag &= (tcflag_t)~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	t.c_cflag &= (tcflag_t)~(CSIZE | PARENB);
	t.c_cflag |= CS8;
	t.c_cc[VMIN] = 1;
	t.c_cc[VTIME] = 0;
	tcsetattr(fd, TCSANOW, &t);
}

/* ------------------------------- read_key -------------------------------
 * Single-keystroke read with minimal ANSI-escape recognition.
 *
 * Returns a non-negative char value, or a negative KEYCODE_* for recognised
 * escape sequences.  In raw mode the kernel can't do select/poll-style
 * non-blocking reads, so we use FIONREAD to peek at the TTY's stdinSize
 * after seeing an ESC byte: if more bytes are already buffered we read
 * the CSI sequence; otherwise we return ESC alone.
 */
static int peek_byte_available(int fd)
{
	int n = 0;
	if (ioctl(fd, FIONREAD, &n) < 0)
		return 0;
	return n > 0;
}

int read_key(int fd, char *buffer, int timeout_ms)
{
	(void)buffer;
	(void)timeout_ms;

	unsigned char c;
	ssize_t n = safe_read(fd, &c, 1);
	if (n <= 0)
		return -1;
	if (c != 0x1B)
		return (int)c;

	/* Lone ESC vs start of CSI: peek the buffer. */
	if (!peek_byte_available(fd))
		return 0x1B;

	if (safe_read(fd, &c, 1) != 1)
		return 0x1B;
	if (c != '[' && c != 'O') {
		/* Some other ESC sequence; swallow the next byte and return ESC. */
		return 0x1B;
	}

	if (safe_read(fd, &c, 1) != 1)
		return 0x1B;

	switch (c) {
	case 'A': return KEYCODE_UP;
	case 'B': return KEYCODE_DOWN;
	case 'C': return KEYCODE_RIGHT;
	case 'D': return KEYCODE_LEFT;
	case 'H': return KEYCODE_HOME;
	case 'F': return KEYCODE_END;
	case '2':
	case '3':
	case '5':
	case '6': {
		unsigned char tilde;
		int code = (c == '2') ? KEYCODE_INSERT
		         : (c == '3') ? KEYCODE_DELETE
		         : (c == '5') ? KEYCODE_PAGEUP
		         :              KEYCODE_PAGEDOWN;
		if (safe_read(fd, &tilde, 1) == 1 && tilde == '~')
			return code;
		return 0x1B;
	}
	default:
		return 0x1B;
	}
}

int safe_read_key(int fd, char *buffer, int timeout_ms)
{
	return read_key(fd, buffer, timeout_ms);
}

/* ------------------------------- getopt32 -------------------------------
 * Minimal getopt32 implementation matching the subset vi.c uses.
 * Supports per-char flag options, ':' for "takes an argument", and '*'
 * for "argument value gets pushed onto an llist_t" (used by -c CMD).
 *
 * Reads bit positions left-to-right through the optstring, skipping the
 * modifier chars when counting bits — matches the OPTBIT_* enum vi.c
 * declares.  Returns the OR of bits for options seen.  Unknown options
 * call bb_show_usage().
 */
static unsigned vgetopt32(char **argv, const char *applet_opts, va_list ap)
{
	struct {
		char  letter;
		char  takes_arg;
		char  is_star;
		void *target;
	} opts[16];
	int nopts = 0;
	const char *p = applet_opts;

	/* busybox optstring modifiers we just need to skip:
	 *   leading '^' / '!' / '+' / '-' — assorted hints we don't implement
	 *   embedded '\0' — modifier section ("-H-h:..." etc.) starts here */
	while (*p == '^' || *p == '!' || *p == '+' || *p == '-')
		p++;

	while (*p && nopts < 16) {
		opts[nopts].letter    = *p++;
		opts[nopts].takes_arg = 0;
		opts[nopts].is_star   = 0;
		opts[nopts].target    = NULL;
		while (*p == ':' || *p == '*' || *p == '+') {
			if (*p == ':') opts[nopts].takes_arg = 1;
			if (*p == '*') opts[nopts].is_star   = 1;
			p++;
		}
		nopts++;
	}

	for (int i = 0; i < nopts; i++) {
		if (opts[i].takes_arg)
			opts[i].target = va_arg(ap, void *);
	}

	unsigned mask = 0;
	int n = 1;
	while (argv[n]) {
		char *a = argv[n];
		if (a[0] != '-' || a[1] == '\0')
			break;
		if (a[0] == '-' && a[1] == '-' && a[2] == '\0') {
			n++;
			break;
		}

		char *q = &a[1];
		while (*q) {
			int found = -1;
			for (int i = 0; i < nopts; i++) {
				if (opts[i].letter == *q) { found = i; break; }
			}
			if (found < 0) {
				fprintf(stderr, "%s: unknown option -%c\n", applet_name, *q);
				bb_show_usage();
			}
			mask |= (1u << found);

			if (opts[found].takes_arg) {
				char *val;
				if (q[1] != '\0') {
					val = q + 1;
				} else if (argv[n + 1]) {
					val = argv[++n];
				} else {
					fprintf(stderr, "%s: option -%c needs an argument\n", applet_name, *q);
					bb_show_usage();
				}
				if (opts[found].is_star) {
					llist_t **head = (llist_t **)opts[found].target;
					llist_t *node = xmalloc(sizeof(llist_t));
					node->data = xstrdup(val);
					node->link = *head;
					*head = node;
				} else if (opts[found].target) {
					*(char **)opts[found].target = xstrdup(val);
				}
				/* this option consumed the rest of the cluster */
				break;
			}
			q++;
		}
		n++;
	}

	/* Compact argv: shift remaining args so applets see them starting at argv[1]. */
	if (n > 1) {
		int dst = 1;
		while (argv[n])
			argv[dst++] = argv[n++];
		argv[dst] = NULL;
	}

	option_mask32 = mask;
	return mask;
}

unsigned getopt32(char **argv, const char *applet_opts, ...)
{
	va_list ap;
	unsigned r;
	va_start(ap, applet_opts);
	r = vgetopt32(argv, applet_opts, ap);
	va_end(ap);
	return r;
}

/* getopt32long — same as getopt32 plus a longopts descriptor string we
 * currently ignore (we don't support `--long-opt` form yet).  va_args
 * after `longopts` line up identically with getopt32's. */
unsigned getopt32long(char **argv, const char *applet_opts,
                      const char *longopts, ...)
{
	va_list ap;
	unsigned r;
	(void)longopts;
	va_start(ap, longopts);
	r = vgetopt32(argv, applet_opts, ap);
	va_end(ap);
	return r;
}

/* Each applet's bin/<applet>/main.c supplies the actual main() that calls
 * <applet>_main.  Keeping main() out of this shared shim means the same
 * libbb_stubs.o source works for vi, wc, head, tail, etc. */
