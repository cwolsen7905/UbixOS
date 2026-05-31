/*
 * libbb.h shim for UbixOS port of busybox vi.
 *
 * Provides the minimum set of busybox declarations, macros, and feature
 * gates that contrib/busybox-vi/vi.c references, mapped onto musl + the
 * UbixOS runtime.  The matching stub implementations live in libbb_stubs.c.
 */
#ifndef UBIX_LIBBB_H
#define UBIX_LIBBB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <setjmp.h>
#include <poll.h>

/* --- feature switches (turn on the editor features we want) --- */
#define ENABLE_FEATURE_VI_COLON           1
#define ENABLE_FEATURE_VI_COLON_EXPAND    1
#define ENABLE_FEATURE_VI_YANKMARK        1
#define ENABLE_FEATURE_VI_SEARCH          1
#define ENABLE_FEATURE_VI_REGEX_SEARCH    0
#define ENABLE_FEATURE_VI_USE_SIGNALS     1
#define ENABLE_FEATURE_VI_DOT_CMD         1
#define ENABLE_FEATURE_VI_READONLY        1
#define ENABLE_FEATURE_VI_SETOPTS         1
#define ENABLE_FEATURE_VI_SET             1
#define ENABLE_FEATURE_VI_WIN_RESIZE      1
#define ENABLE_FEATURE_VI_ASK_TERMINAL    0
#define ENABLE_FEATURE_VI_UNDO            1
#define ENABLE_FEATURE_VI_UNDO_QUEUE      1
#define ENABLE_FEATURE_VI_VERBOSE_STATUS  1
#define ENABLE_FEATURE_VI_8BIT            0
#define ENABLE_FEATURE_VI_CRASHME         0

/* --- per-applet feature switches for coreutils etc. --- */
#define ENABLE_FEATURE_WC_LARGE           1
#define ENABLE_FEATURE_FANCY_HEAD         1
#define ENABLE_FEATURE_FANCY_TAIL         1
#define ENABLE_FEATURE_CLEAN_UP           0

#define ENABLE_PLATFORM_MINGW32           0
#define ENABLE_LOCALE_SUPPORT             0
#define ENABLE_FEATURE_ASSUME_UNICODE     0
#define ENABLE_FEATURE_EDITING            0
#define ENABLE_FEATURE_EDITING_VI         0
#define ENABLE_FEATURE_EDITING_FANCY_KEYS 0
#define ENABLE_LONG_OPTS                  0
#define ENABLE_DESKTOP                    0

#define CONFIG_FEATURE_VI_MAX_LEN         4096
#define CONFIG_FEATURE_VI_UNDO_QUEUE_MAX  256

/* IF_x(...) macros — variadic so the busybox idiom of passing leading
 * commas works (e.g. IF_FEATURE_VI_COLON(, &foo) expands to either
 * ", &foo" or nothing). */
#define IF_VI(...) __VA_ARGS__

#if ENABLE_FEATURE_VI_COLON
# define IF_FEATURE_VI_COLON(...) __VA_ARGS__
#else
# define IF_FEATURE_VI_COLON(...)
#endif
#if ENABLE_FEATURE_VI_SEARCH
# define IF_FEATURE_VI_SEARCH(...) __VA_ARGS__
#else
# define IF_FEATURE_VI_SEARCH(...)
#endif
#if ENABLE_FEATURE_VI_READONLY
# define IF_FEATURE_VI_READONLY(...) __VA_ARGS__
#else
# define IF_FEATURE_VI_READONLY(...)
#endif
#if ENABLE_FEATURE_VI_SETOPTS
# define IF_FEATURE_VI_SETOPTS(...) __VA_ARGS__
#else
# define IF_FEATURE_VI_SETOPTS(...)
#endif
#if ENABLE_FEATURE_VI_ASK_TERMINAL
# define IF_FEATURE_VI_ASK_TERMINAL(...) __VA_ARGS__
#else
# define IF_FEATURE_VI_ASK_TERMINAL(...)
#endif
#if ENABLE_FEATURE_VI_CRASHME
# define IF_FEATURE_VI_CRASHME(...) __VA_ARGS__
#else
# define IF_FEATURE_VI_CRASHME(...)
#endif

#if ENABLE_FEATURE_FANCY_TAIL
# define IF_FEATURE_FANCY_TAIL(...) __VA_ARGS__
#else
# define IF_FEATURE_FANCY_TAIL(...)
#endif
#if ENABLE_FEATURE_FANCY_HEAD
# define IF_FEATURE_FANCY_HEAD(...) __VA_ARGS__
#else
# define IF_FEATURE_FANCY_HEAD(...)
#endif

/* C boolean shims used by busybox */
#ifndef TRUE
# define TRUE  1
#endif
#ifndef FALSE
# define FALSE 0
#endif

/* generic attribute / qualifier macros */
#define FAST_FUNC
#define NORETURN          __attribute__((noreturn))
#define ALIGN1            __attribute__((aligned(1)))
#define ALWAYS_INLINE     __attribute__((always_inline)) inline
#define UNUSED_PARAM      __attribute__((unused))
#define MAIN_EXTERNALLY_VISIBLE

/* busybox uses these tiny ints to save space; on UbixOS just alias them */
typedef signed char        smallint;
typedef unsigned char      smalluint;

/* singly-linked list — vi only uses link/data */
typedef struct llist_t {
	struct llist_t *link;
	char           *data;
} llist_t;

/* helpful macros */
#ifndef ARRAY_SIZE
# define ARRAY_SIZE(x)   (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef MIN
# define MIN(a,b)        ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
# define MAX(a,b)        ((a) > (b) ? (a) : (b))
#endif

/* --- globals indirection ---
 * vi.c defines `struct globals { ... }` then uses `#define G (*ptr_to_globals)`.
 * struct globals is forward-declared here; vi.c provides the definition.
 * The pointer is allocated at startup via SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))). */
struct globals;
extern struct globals *ptr_to_globals;
#define SET_PTR_TO_GLOBALS(p) \
	do { *(struct globals **)&ptr_to_globals = (struct globals *)(p); } while (0)
#define barrier()  __asm__ __volatile__ ("" ::: "memory")

/* --- KEYCODE constants used by read_key()/safe_read_key() --- */
enum {
	KEYCODE_BUFFER_SIZE = 16,
	KEYCODE_UP        = -2,
	KEYCODE_DOWN      = -3,
	KEYCODE_RIGHT     = -4,
	KEYCODE_LEFT      = -5,
	KEYCODE_HOME      = -6,
	KEYCODE_END       = -7,
	KEYCODE_INSERT    = -8,
	KEYCODE_DELETE    = -9,
	KEYCODE_PAGEUP    = -10,
	KEYCODE_PAGEDOWN  = -11,
	KEYCODE_CURSOR_POS = -20,  /* response to \E[6n — not generated here */
	KEYCODE_FUN       = -32,   /* base for F1..F12 (we don't generate these) */
};

/* termios flags accepted by set_termios_to_raw */
#define TERMIOS_RAW_CRNL  0x1
#define TERMIOS_RAW_INPUT 0x2

/* --- error/version macros expected by vi.c --- */
#define BB_VER             "busybox-vi 1.36.1 (UbixOS port)"
#define STRERROR_FMT       "%s"
#define STRERROR_ERRNO     ,strerror(errno)

/* --- libbb function stubs (defined in libbb_stubs.c) --- */
void  *xmalloc(size_t size);
void  *xrealloc(void *ptr, size_t size);
void  *xzalloc(size_t size);
char  *xstrdup(const char *s);
char  *xstrndup(const char *s, size_t n);
char  *xasprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
char  *xmalloc_open_read_close(const char *filename, size_t *maxsz_p);
ssize_t safe_read(int fd, void *buf, size_t count);
ssize_t full_read(int fd, void *buf, size_t count);
ssize_t full_write(int fd, const void *buf, size_t count);
char  *strchrnul(const char *s, int c);
void  *memrchr(const void *s, int c, size_t n);
int    safe_poll(struct pollfd *ufds, nfds_t nfds, int timeout);
int    fputs_stdout(const char *s);
int    get_terminal_width_height(int fd, unsigned *width, unsigned *height);
void   bb_simple_perror_msg(const char *s);
FILE  *fopen_or_warn_stdin(const char *filename);
int    fclose_if_not_stdin(FILE *fp);
void   fflush_stdout_and_exit(int status) NORETURN;
char  *xmalloc_fgets(FILE *fp);
void   die_if_ferror_stdout(void);

struct suffix_mult {
	char     suffix[4];
	unsigned mult;
};
extern const struct suffix_mult bkm_suffixes[];
unsigned long long xatoul_sfx(const char *numstr, const struct suffix_mult *suffixes);
unsigned xatou_sfx(const char *numstr, const struct suffix_mult *suffixes);
int fdprintf(int fd, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int open_or_warn_stdin(const char *filename);
off_t xlseek(int fd, off_t offset, int whence);
void  xwrite(int fd, const void *buf, size_t count);
void   bb_error_msg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void   bb_perror_msg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void   bb_perror_nomsg_and_die(void) NORETURN;
void   xmove_fd(int from, int to);
off_t  bb_copyfd_size(int fd1, int fd2, off_t size);
void   setup_common_bufsiz(void);

#define COMMON_BUFSIZE 1024
extern char bb_common_bufsiz1[COMMON_BUFSIZE];

extern const char bb_msg_standard_input[];
extern const char bb_msg_read_error[];
void   bb_putchar(int c);
void   bb_show_usage(void) NORETURN;
void   bb_simple_error_msg_and_die(const char *s) NORETURN;
unsigned bb_strtou(const char *arg, char **endp, int base);
char  *concat_path_file(const char *path, const char *filename);
char  *skip_whitespace(const char *s);
char  *skip_non_whitespace(const char *s);
int    index_in_strings(const char *strings, const char *key);
void  *llist_pop(llist_t **head);
void   fflush_all(void);
void   tcsetattr_stdin_TCSANOW(const struct termios *tio);
void   set_termios_to_raw(int fd, struct termios *orig_out, int flags);
int    read_key(int fd, char *buffer, int timeout_ms);
int    safe_read_key(int fd, char *buffer, int timeout_ms);
unsigned getopt32(char **argv, const char *applet_opts, ...);

extern const char *applet_name;

#endif /* UBIX_LIBBB_H */
