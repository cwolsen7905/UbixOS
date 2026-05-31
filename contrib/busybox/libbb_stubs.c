/*
 * libbb stub implementations for the UbixOS port of busybox vi.
 * Provides the minimum support functions vi.c imports from busybox's
 * libbb.  Kept tiny on purpose — anything fancier than malloc-or-die
 * gets revisited only if vi grows a dependency on it.
 */
#include "libbb.h"

/* The active applet sets applet_name in its own main() wrapper. */
const char *applet_name = "busybox";
const char bb_msg_standard_input[] = "standard input";
const char bb_msg_read_error[]     = "read error";

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

unsigned xatou_sfx(const char *numstr, const struct suffix_mult *suffixes)
{
	unsigned long long v = xatoul_sfx(numstr, suffixes);
	if (v > UINT_MAX)
		bb_simple_error_msg_and_die(numstr);
	return (unsigned)v;
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
unsigned getopt32(char **argv, const char *applet_opts, ...)
{
	va_list ap;
	struct {
		char  letter;
		char  takes_arg;
		char  is_star;
		void *target;
	} opts[16];
	int nopts = 0;
	const char *p = applet_opts;

	while (*p && nopts < 16) {
		opts[nopts].letter    = *p++;
		opts[nopts].takes_arg = 0;
		opts[nopts].is_star   = 0;
		opts[nopts].target    = NULL;
		while (*p == ':' || *p == '*') {
			if (*p == ':') opts[nopts].takes_arg = 1;
			if (*p == '*') opts[nopts].is_star   = 1;
			p++;
		}
		nopts++;
	}

	va_start(ap, applet_opts);
	for (int i = 0; i < nopts; i++) {
		if (opts[i].takes_arg)
			opts[i].target = va_arg(ap, void *);
	}
	va_end(ap);

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
				fprintf(stderr, "unknown option -%c\n", *q);
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
					fprintf(stderr, "option -%c needs an argument\n", *q);
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

	/* Compact argv: shift remaining args so vi.c sees them starting at argv[1]. */
	if (n > 1) {
		int dst = 1;
		while (argv[n])
			argv[dst++] = argv[n++];
		argv[dst] = NULL;
	}

	return mask;
}

/* Each applet's bin/<applet>/main.c supplies the actual main() that calls
 * <applet>_main.  Keeping main() out of this shared shim means the same
 * libbb_stubs.o source works for vi, wc, head, tail, etc. */
