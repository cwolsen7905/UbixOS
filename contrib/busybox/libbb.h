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
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <sys/sysmacros.h>

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
#define ENABLE_FEATURE_GREP_CONTEXT       1
#define ENABLE_EXTRA_COMPAT               0
#define ENABLE_FEATURE_GREP_FGREP_ALIAS   1
#define ENABLE_FEATURE_GREP_EGREP_ALIAS   1
#define ENABLE_FGREP                      1
#define ENABLE_EGREP                      1

/* find: enable common predicates, skip the ones that pull substantial
 * extra dependencies (exec/delete/regex/SELinux). */
#define ENABLE_FEATURE_FIND_PRINT0        1
#define ENABLE_FEATURE_FIND_MTIME         1
#define ENABLE_FEATURE_FIND_MMIN          1
#define ENABLE_FEATURE_FIND_PERM          0
#define ENABLE_FEATURE_FIND_TYPE          1
#define ENABLE_FEATURE_FIND_XDEV          1
#define ENABLE_FEATURE_FIND_MAXDEPTH      1
#define ENABLE_FEATURE_FIND_NEWER         1
#define ENABLE_FEATURE_FIND_INUM          1
#define ENABLE_FEATURE_FIND_USER          0
#define ENABLE_FEATURE_FIND_GROUP         0
#define ENABLE_FEATURE_FIND_NOT           1
#define ENABLE_FEATURE_FIND_DEPTH         1
#define ENABLE_FEATURE_FIND_PAREN         1
#define ENABLE_FEATURE_FIND_SIZE          1
#define ENABLE_FEATURE_FIND_PRUNE         1
#define ENABLE_FEATURE_FIND_QUIT          1
#define ENABLE_FEATURE_FIND_DELETE        0
#define ENABLE_FEATURE_FIND_PATH          1
#define ENABLE_FEATURE_FIND_REGEX         0
#define ENABLE_FEATURE_FIND_CONTEXT       0
#define ENABLE_FEATURE_FIND_LINKS         1
#define ENABLE_FEATURE_FIND_EMPTY         1
#define ENABLE_FEATURE_FIND_EXEC          0
#define ENABLE_FEATURE_FIND_EXEC_PLUS     0
#define ENABLE_FEATURE_FIND_EXECUTABLE    0
#define ENABLE_FEATURE_FIND_AMIN          1
#define ENABLE_FEATURE_FIND_CMIN          1
#define ENABLE_FEATURE_FIND_ATIME         1
#define ENABLE_FEATURE_FIND_CTIME         1
#define ENABLE_FEATURE_FIND_SAMEFILE      0

/* coreutils replacements: cat, uname, ls, stat */
#define ENABLE_CAT                        1
#define ENABLE_FEATURE_CATN               1
#define ENABLE_FEATURE_CATV               1
#define ENABLE_UNAME                      1
#define ENABLE_BB_ARCH                    0
#define ENABLE_FEDORA_COMPAT              0
#define ENABLE_FEATURE_LS_FILETYPES       1
#define ENABLE_FEATURE_LS_FOLLOWLINKS     1
#define ENABLE_FEATURE_LS_RECURSIVE       1
#define ENABLE_FEATURE_LS_SORTFILES       1
#define ENABLE_FEATURE_LS_TIMESTAMPS      1
#define ENABLE_FEATURE_LS_USERNAME        0
#define ENABLE_FEATURE_LS_WIDTH           1
#define ENABLE_FEATURE_LS_COLOR           0
#define ENABLE_FEATURE_LS_COLOR_IS_DEFAULT 0
#define ENABLE_FEATURE_HUMAN_READABLE     1
#define ENABLE_FEATURE_AUTOWIDTH          1
#define ENABLE_FEATURE_STAT_FORMAT        1
#define ENABLE_FEATURE_STAT_FILESYSTEM    0

/* text-processing batch (sort, cut, tr, uniq, more) */
#define ENABLE_FEATURE_SORT_BIG           1
#define ENABLE_FEATURE_SORT_OPTIMIZE_MEMORY 0
#define ENABLE_FEATURE_CUT_REGEX          1
#define ENABLE_FEATURE_TR_CLASSES         1
#define ENABLE_FEATURE_TR_EQUIV           1

/* file-ops batch (cp, mkdir, rm, mv, touch) */
#define ENABLE_FEATURE_VERBOSE                1
#define ENABLE_FEATURE_CP_LONG_OPTIONS        1
#define ENABLE_FEATURE_CP_REFLINK             0
#define ENABLE_FEATURE_NON_POSIX_CP           1
#define ENABLE_FEATURE_PRESERVE_HARDLINKS     0
#define ENABLE_FEATURE_VERBOSE_CP_MESSAGE     1
#define ENABLE_FEATURE_RM_INTERACTIVE         1
#define ENABLE_FEATURE_MV_LONG_OPTIONS        0
#define ENABLE_FEATURE_MKDIR_LONG_OPTIONS     0
#define ENABLE_FEATURE_TOUCH_NODEREF          0
#define ENABLE_FEATURE_TOUCH_SUSV3            0
#define ENABLE_FEATURE_DATE_ISOFMT            0
#define ENABLE_INSTALL                        0
#define ENABLE_FEATURE_INSTALL_LONG_OPTIONS   0

/* small-utilities batch (env, sleep, date, basename, dirname, which) */
#define ENABLE_FEATURE_FANCY_SLEEP            1
#define ENABLE_FEATURE_DATE_COMPAT            1
#define ENABLE_FEATURE_DATE_ISOFMT            1
#define ENABLE_FEATURE_DATE_NANO              0
#define ENABLE_FEATURE_DATE_BIRTHDAY          0
#define ENABLE_FEATURE_ENV_DEFAULT_LONG_OPTIONS 0
#define ENABLE_FTPD                       0
#define ENABLE_SELINUX                    0
#define CONFIG_UNAME_OSNAME               "UbixOS"

/* Width strings/macros used by ls.c when laying out columns with
 * make_human_readable_str.  Upstream picks these per-platform; on
 * a 32-bit i386 build the off_t is long long, so use the wider
 * formatter and reserve 12 columns for sizes. */
#define HUMAN_READABLE_MAX_WIDTH      7
#define HUMAN_READABLE_MAX_WIDTH_STR  "7"
#define OFF_FMT                       "ll"

#define ENABLE_FEATURE_LESS_BRACKETS      0
#define ENABLE_FEATURE_LESS_DASHCMD       1
#define ENABLE_FEATURE_LESS_ENV           0
#define ENABLE_FEATURE_LESS_FLAGS         1
#define ENABLE_FEATURE_LESS_LINENUMS      1
#define ENABLE_FEATURE_LESS_MARKS         1
#define ENABLE_FEATURE_LESS_RAW           1
#define ENABLE_FEATURE_LESS_REGEXP        1
#define ENABLE_FEATURE_LESS_TRUNCATE      1
#define ENABLE_FEATURE_LESS_WINCH         0
#define ENABLE_FEATURE_LESS_ASK_TERMINAL  0
#define CONFIG_FEATURE_LESS_MAXLINES      9999999

/* LONE_DASH(s): true when s is exactly "-". */
#define LONE_DASH(s)  ((s)[0] == '-' && (s)[1] == '\0')
#define LONE_CHAR(s, c)  ((s)[0] == (c) && (s)[1] == '\0')

/* upstream uses this to align a suffix table; harmless on UbixOS i386. */
#define ALIGN_SUFFIX  __attribute__((aligned(__alignof__(struct suffix_mult))))

/* Compile-time assertion used by busybox to check struct sizes etc. */
#define BUILD_BUG_ON(cond)  ((void)sizeof(char[1 - 2*!!(cond)]))

/* busybox shortcuts for exit() with the standard exit codes. */
#define exit_SUCCESS()  exit(EXIT_SUCCESS)
#define exit_FAILURE()  exit(EXIT_FAILURE)
#define fflush_stdout_and_exit_SUCCESS()  fflush_stdout_and_exit(EXIT_SUCCESS)
#define fflush_stdout_and_exit_FAILURE()  fflush_stdout_and_exit(EXIT_FAILURE)

/* IF_SELINUX / IF_LONG_OPTS gates */
#if ENABLE_SELINUX
# define IF_SELINUX(...) __VA_ARGS__
#else
# define IF_SELINUX(...)
#endif
#if ENABLE_LONG_OPTS
# define IF_LONG_OPTS(...) __VA_ARGS__
#else
# define IF_LONG_OPTS(...)
#endif

#define ENABLE_PLATFORM_MINGW32           0
#define ENABLE_LOCALE_SUPPORT             0
#define ENABLE_FEATURE_ASSUME_UNICODE     0
#define ENABLE_FEATURE_EDITING            0
#define ENABLE_FEATURE_EDITING_VI         0
#define ENABLE_FEATURE_EDITING_FANCY_KEYS 0
#define ENABLE_LONG_OPTS                  1
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
#if ENABLE_FEATURE_GREP_CONTEXT
# define IF_FEATURE_GREP_CONTEXT(...) __VA_ARGS__
#else
# define IF_FEATURE_GREP_CONTEXT(...)
#endif
#if ENABLE_EXTRA_COMPAT
# define IF_EXTRA_COMPAT(...) __VA_ARGS__
#else
# define IF_EXTRA_COMPAT(...)
#endif

#if ENABLE_FEATURE_LESS_REGEXP
# define IF_FEATURE_LESS_REGEXP(...) __VA_ARGS__
#else
# define IF_FEATURE_LESS_REGEXP(...)
#endif
#if ENABLE_FEATURE_LESS_TRUNCATE
# define IF_FEATURE_LESS_TRUNCATE(...) __VA_ARGS__
#else
# define IF_FEATURE_LESS_TRUNCATE(...)
#endif
#if ENABLE_FEATURE_LESS_RAW
# define IF_FEATURE_LESS_RAW(...) __VA_ARGS__
#else
# define IF_FEATURE_LESS_RAW(...)
#endif
#if ENABLE_FEATURE_LESS_BRACKETS
# define IF_FEATURE_LESS_BRACKETS(...) __VA_ARGS__
#else
# define IF_FEATURE_LESS_BRACKETS(...)
#endif
#if ENABLE_FEATURE_LESS_DASHCMD
# define IF_FEATURE_LESS_DASHCMD(...) __VA_ARGS__
#else
# define IF_FEATURE_LESS_DASHCMD(...)
#endif
#if ENABLE_FEATURE_LESS_FLAGS
# define IF_FEATURE_LESS_FLAGS(...) __VA_ARGS__
#else
# define IF_FEATURE_LESS_FLAGS(...)
#endif
#if ENABLE_FEATURE_LESS_LINENUMS
# define IF_FEATURE_LESS_LINENUMS(...) __VA_ARGS__
#else
# define IF_FEATURE_LESS_LINENUMS(...)
#endif
#if ENABLE_FEATURE_LESS_MARKS
# define IF_FEATURE_LESS_MARKS(...) __VA_ARGS__
#else
# define IF_FEATURE_LESS_MARKS(...)
#endif
#if ENABLE_FEATURE_LESS_ENV
# define IF_FEATURE_LESS_ENV(...) __VA_ARGS__
#else
# define IF_FEATURE_LESS_ENV(...)
#endif
#if ENABLE_FEATURE_LESS_WINCH
# define IF_FEATURE_LESS_WINCH(...) __VA_ARGS__
#else
# define IF_FEATURE_LESS_WINCH(...)
#endif
#if ENABLE_FEATURE_LESS_ASK_TERMINAL
# define IF_FEATURE_LESS_ASK_TERMINAL(...) __VA_ARGS__
#else
# define IF_FEATURE_LESS_ASK_TERMINAL(...)
#endif

/* coreutils gates */
#define IF_CAT(...)              __VA_ARGS__
#define IF_UNAME(...)            __VA_ARGS__

#if ENABLE_FEATURE_CATN
# define IF_FEATURE_CATN(...)    __VA_ARGS__
#else
# define IF_FEATURE_CATN(...)
#endif
#if ENABLE_FEATURE_CATV
# define IF_FEATURE_CATV(...)    __VA_ARGS__
#else
# define IF_FEATURE_CATV(...)
#endif
#if ENABLE_BB_ARCH
# define IF_BB_ARCH(...)         __VA_ARGS__
#else
# define IF_BB_ARCH(...)
#endif

#if ENABLE_FEATURE_LS_FILETYPES
# define IF_FEATURE_LS_FILETYPES(...) __VA_ARGS__
#else
# define IF_FEATURE_LS_FILETYPES(...)
#endif
#if ENABLE_FEATURE_LS_FOLLOWLINKS
# define IF_FEATURE_LS_FOLLOWLINKS(...) __VA_ARGS__
#else
# define IF_FEATURE_LS_FOLLOWLINKS(...)
#endif
#if ENABLE_FEATURE_LS_RECURSIVE
# define IF_FEATURE_LS_RECURSIVE(...) __VA_ARGS__
#else
# define IF_FEATURE_LS_RECURSIVE(...)
#endif
#if ENABLE_FEATURE_LS_SORTFILES
# define IF_FEATURE_LS_SORTFILES(...) __VA_ARGS__
#else
# define IF_FEATURE_LS_SORTFILES(...)
#endif
#if ENABLE_FEATURE_LS_TIMESTAMPS
# define IF_FEATURE_LS_TIMESTAMPS(...) __VA_ARGS__
#else
# define IF_FEATURE_LS_TIMESTAMPS(...)
#endif
#if ENABLE_FEATURE_LS_USERNAME
# define IF_FEATURE_LS_USERNAME(...) __VA_ARGS__
#else
# define IF_FEATURE_LS_USERNAME(...)
#endif
#if ENABLE_FEATURE_LS_WIDTH
# define IF_FEATURE_LS_WIDTH(...) __VA_ARGS__
#else
# define IF_FEATURE_LS_WIDTH(...)
#endif
#if ENABLE_FEATURE_LS_COLOR
# define IF_FEATURE_LS_COLOR(...) __VA_ARGS__
#else
# define IF_FEATURE_LS_COLOR(...)
#endif
#if ENABLE_FEATURE_HUMAN_READABLE
# define IF_FEATURE_HUMAN_READABLE(...) __VA_ARGS__
#else
# define IF_FEATURE_HUMAN_READABLE(...)
#endif
#if ENABLE_FEATURE_AUTOWIDTH
# define IF_FEATURE_AUTOWIDTH(...) __VA_ARGS__
#else
# define IF_FEATURE_AUTOWIDTH(...)
#endif
#if ENABLE_FEATURE_STAT_FORMAT
# define IF_FEATURE_STAT_FORMAT(...) __VA_ARGS__
#else
# define IF_FEATURE_STAT_FORMAT(...)
#endif
#if ENABLE_FEATURE_STAT_FILESYSTEM
# define IF_FEATURE_STAT_FILESYSTEM(...) __VA_ARGS__
#else
# define IF_FEATURE_STAT_FILESYSTEM(...)
#endif
#if ENABLE_FEATURE_SORT_BIG
# define IF_FEATURE_SORT_BIG(...) __VA_ARGS__
#else
# define IF_FEATURE_SORT_BIG(...)
#endif
#if ENABLE_FEATURE_CUT_REGEX
# define IF_FEATURE_CUT_REGEX(...) __VA_ARGS__
#else
# define IF_FEATURE_CUT_REGEX(...)
#endif
#if ENABLE_FEATURE_TR_CLASSES
# define IF_FEATURE_TR_CLASSES(...) __VA_ARGS__
#else
# define IF_FEATURE_TR_CLASSES(...)
#endif
#if ENABLE_FEATURE_TR_EQUIV
# define IF_FEATURE_TR_EQUIV(...) __VA_ARGS__
#else
# define IF_FEATURE_TR_EQUIV(...)
#endif
#if ENABLE_FEATURE_VERBOSE
# define IF_FEATURE_VERBOSE(...) __VA_ARGS__
#else
# define IF_FEATURE_VERBOSE(...)
#endif
#if ENABLE_FEATURE_CP_LONG_OPTIONS
# define IF_FEATURE_CP_LONG_OPTIONS(...) __VA_ARGS__
#else
# define IF_FEATURE_CP_LONG_OPTIONS(...)
#endif
#if ENABLE_FEATURE_CP_REFLINK
# define IF_FEATURE_CP_REFLINK(...) __VA_ARGS__
#else
# define IF_FEATURE_CP_REFLINK(...)
#endif
#if ENABLE_FEATURE_PRESERVE_HARDLINKS
# define IF_FEATURE_PRESERVE_HARDLINKS(...) __VA_ARGS__
#else
# define IF_FEATURE_PRESERVE_HARDLINKS(...)
#endif
#if ENABLE_FEATURE_VERBOSE_CP_MESSAGE
# define IF_FEATURE_VERBOSE_CP_MESSAGE(...) __VA_ARGS__
#else
# define IF_FEATURE_VERBOSE_CP_MESSAGE(...)
#endif
#if ENABLE_FEATURE_MV_LONG_OPTIONS
# define IF_FEATURE_MV_LONG_OPTIONS(...) __VA_ARGS__
#else
# define IF_FEATURE_MV_LONG_OPTIONS(...)
#endif
#if ENABLE_FEATURE_MKDIR_LONG_OPTIONS
# define IF_FEATURE_MKDIR_LONG_OPTIONS(...) __VA_ARGS__
#else
# define IF_FEATURE_MKDIR_LONG_OPTIONS(...)
#endif
#if ENABLE_FEATURE_TOUCH_NODEREF
# define IF_FEATURE_TOUCH_NODEREF(...) __VA_ARGS__
#else
# define IF_FEATURE_TOUCH_NODEREF(...)
#endif
#if ENABLE_FEATURE_TOUCH_SUSV3
# define IF_FEATURE_TOUCH_SUSV3(...) __VA_ARGS__
# define IF_NOT_FEATURE_TOUCH_SUSV3(...)
#else
# define IF_FEATURE_TOUCH_SUSV3(...)
# define IF_NOT_FEATURE_TOUCH_SUSV3(...) __VA_ARGS__
#endif
#if ENABLE_FEATURE_NON_POSIX_CP
# define IF_FEATURE_NON_POSIX_CP(...) __VA_ARGS__
#else
# define IF_FEATURE_NON_POSIX_CP(...)
#endif
#if ENABLE_FEATURE_FANCY_SLEEP
# define IF_FEATURE_FANCY_SLEEP(...) __VA_ARGS__
#else
# define IF_FEATURE_FANCY_SLEEP(...)
#endif
#if ENABLE_FEATURE_DATE_COMPAT
# define IF_FEATURE_DATE_COMPAT(...) __VA_ARGS__
#else
# define IF_FEATURE_DATE_COMPAT(...)
#endif
#if ENABLE_FEATURE_DATE_ISOFMT
# define IF_FEATURE_DATE_ISOFMT(...) __VA_ARGS__
#else
# define IF_FEATURE_DATE_ISOFMT(...)
#endif
#if ENABLE_FEATURE_DATE_NANO
# define IF_FEATURE_DATE_NANO(...) __VA_ARGS__
#else
# define IF_FEATURE_DATE_NANO(...)
#endif
#if ENABLE_FEATURE_TIMEZONE
# define IF_FEATURE_TIMEZONE(...) __VA_ARGS__
#else
# define IF_FEATURE_TIMEZONE(...)
#endif

/* IF_FEATURE_FIND_*(t): variadic so wrappers like
 *     IF_FEATURE_FIND_PATH(ACTS(path, ...))
 * pass commas through correctly when enabled. */
#define _BB_FIND_GATE(name) \
	_BB_FIND_GATE2(ENABLE_FEATURE_FIND_##name)
#define _BB_FIND_GATE2(v)  _BB_FIND_GATE3(v)
#define _BB_FIND_GATE3(v)  _BB_FIND_GATE_##v
#define _BB_FIND_GATE_1(...)  __VA_ARGS__
#define _BB_FIND_GATE_0(...)

#define IF_FEATURE_FIND_PRINT0(...)     _BB_FIND_GATE(PRINT0)(__VA_ARGS__)
#define IF_FEATURE_FIND_MTIME(...)      _BB_FIND_GATE(MTIME)(__VA_ARGS__)
#define IF_FEATURE_FIND_MMIN(...)       _BB_FIND_GATE(MMIN)(__VA_ARGS__)
#define IF_FEATURE_FIND_PERM(...)       _BB_FIND_GATE(PERM)(__VA_ARGS__)
#define IF_FEATURE_FIND_TYPE(...)       _BB_FIND_GATE(TYPE)(__VA_ARGS__)
#define IF_FEATURE_FIND_XDEV(...)       _BB_FIND_GATE(XDEV)(__VA_ARGS__)
#define IF_FEATURE_FIND_MAXDEPTH(...)   _BB_FIND_GATE(MAXDEPTH)(__VA_ARGS__)
#define IF_FEATURE_FIND_NEWER(...)      _BB_FIND_GATE(NEWER)(__VA_ARGS__)
#define IF_FEATURE_FIND_INUM(...)       _BB_FIND_GATE(INUM)(__VA_ARGS__)
#define IF_FEATURE_FIND_USER(...)       _BB_FIND_GATE(USER)(__VA_ARGS__)
#define IF_FEATURE_FIND_GROUP(...)      _BB_FIND_GATE(GROUP)(__VA_ARGS__)
#define IF_FEATURE_FIND_NOT(...)        _BB_FIND_GATE(NOT)(__VA_ARGS__)
#define IF_FEATURE_FIND_DEPTH(...)      _BB_FIND_GATE(DEPTH)(__VA_ARGS__)
#define IF_FEATURE_FIND_PAREN(...)      _BB_FIND_GATE(PAREN)(__VA_ARGS__)
#define IF_FEATURE_FIND_SIZE(...)       _BB_FIND_GATE(SIZE)(__VA_ARGS__)
#define IF_FEATURE_FIND_PRUNE(...)      _BB_FIND_GATE(PRUNE)(__VA_ARGS__)
#define IF_FEATURE_FIND_QUIT(...)       _BB_FIND_GATE(QUIT)(__VA_ARGS__)
#define IF_FEATURE_FIND_DELETE(...)     _BB_FIND_GATE(DELETE)(__VA_ARGS__)
#define IF_FEATURE_FIND_PATH(...)       _BB_FIND_GATE(PATH)(__VA_ARGS__)
#define IF_FEATURE_FIND_REGEX(...)      _BB_FIND_GATE(REGEX)(__VA_ARGS__)
#define IF_FEATURE_FIND_CONTEXT(...)    _BB_FIND_GATE(CONTEXT)(__VA_ARGS__)
#define IF_FEATURE_FIND_LINKS(...)      _BB_FIND_GATE(LINKS)(__VA_ARGS__)
#define IF_FEATURE_FIND_EMPTY(...)      _BB_FIND_GATE(EMPTY)(__VA_ARGS__)
#define IF_FEATURE_FIND_EXEC(...)       _BB_FIND_GATE(EXEC)(__VA_ARGS__)
#define IF_FEATURE_FIND_EXEC_PLUS(...)  _BB_FIND_GATE(EXEC_PLUS)(__VA_ARGS__)
#define IF_FEATURE_FIND_EXECUTABLE(...) _BB_FIND_GATE(EXECUTABLE)(__VA_ARGS__)
#define IF_FEATURE_FIND_AMIN(...)       _BB_FIND_GATE(AMIN)(__VA_ARGS__)
#define IF_FEATURE_FIND_CMIN(...)       _BB_FIND_GATE(CMIN)(__VA_ARGS__)
#define IF_FEATURE_FIND_ATIME(...)      _BB_FIND_GATE(ATIME)(__VA_ARGS__)
#define IF_FEATURE_FIND_CTIME(...)      _BB_FIND_GATE(CTIME)(__VA_ARGS__)
#define IF_FEATURE_FIND_SAMEFILE(...)   _BB_FIND_GATE(SAMEFILE)(__VA_ARGS__)
#define IF_DESKTOP(...)                 _BB_FIND_GATE_0(__VA_ARGS__)

/* IF_NOT_FEATURE_FIND_*(t) — inverse gates. */
#define _BB_FIND_NGATE(name)  _BB_FIND_NGATE2(ENABLE_FEATURE_FIND_##name)
#define _BB_FIND_NGATE2(v)    _BB_FIND_NGATE3(v)
#define _BB_FIND_NGATE3(v)    _BB_FIND_NGATE_##v
#define _BB_FIND_NGATE_1(...)
#define _BB_FIND_NGATE_0(...) __VA_ARGS__

#define IF_NOT_FEATURE_FIND_MAXDEPTH(...) _BB_FIND_NGATE(MAXDEPTH)(__VA_ARGS__)
#define IF_NOT_FEATURE_FIND_DEPTH(...)    _BB_FIND_NGATE(DEPTH)(__VA_ARGS__)
#define IF_NOT_FEATURE_FIND_XDEV(...)     _BB_FIND_NGATE(XDEV)(__VA_ARGS__)

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
#define NOINLINE          __attribute__((noinline))
#define ALIGN2            __attribute__((aligned(2)))
#define ALIGN4            __attribute__((aligned(4)))
#define PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN
#define POP_SAVED_FUNCTION_VISIBILITY

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
char  *strcasestr(const char *haystack, const char *needle);
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
unsigned xatou(const char *numstr);
unsigned xatou_range_sfx(const char *numstr, unsigned lo, unsigned hi,
                         const struct suffix_mult *suffixes);
long  bb_strtol(const char *arg, char **endp, int base);
long long bb_strtoll(const char *arg, char **endp, int base);
extern const char bb_msg_invalid_date[];
extern const char bb_default_root_path[];
extern const char bb_msg_write_error[];

/* sed needs these too */
void   llist_add_to_end(llist_t **list_head, void *data);
FILE  *xfdopen_for_write(int fd);
FILE  *xfopen_for_write(const char *path);
int    xmkstemp(char *template);
void   xrename(const char *oldpath, const char *newpath);
void   overlapping_strcpy(char *dst, const char *src);

/* die_func: optional cleanup hook bb_show_usage and xfunc_die can call.
 * sed sets it to flush its output buffer before exiting. */
extern void (*die_func)(void);

/* small-util support */
#define ENABLE_ASH_SLEEP                      0
#define ENABLE_FEATURE_TIMEZONE               0
/* String literal so which.c's sizeof() works.  bb_default_root_path
 * remains as the named symbol for callers that want a runtime pointer. */
#define BB_PATH_ROOT_PATH                     "/usr/bin:/bin:/usr/sbin:/sbin"
/* BB_EXECVP_or_die: function provided by contrib/busybox/libbb/executable.c.
 * env / which / etc. pull executable.o into their link line.  BB_EXECVP is
 * the underlying macro upstream uses — defined here so executable.c's
 * BB_EXECVP_or_die compiles. */
#define BB_EXECVP(prog, cmd)  execvp(prog, cmd)
void BB_EXECVP_or_die(char **argv);

extern char **environ;

#define ENABLE_FLOAT_DURATION 0
typedef unsigned duration_t;
#define DURATION_FMT "u"
#define sleep_for_duration(duration) sleep(duration)
duration_t parse_duration_str(char *str);
int        index_in_substrings(const char *strings, const char *key);
int        parse_datestr(const char *date_str, struct tm *tm_time);
time_t     validate_tm_time(const char *date_str, struct tm *tm_time);
int        file_is_executable(const char *name);
char      *find_executable(const char *filename, char **PATHp);
void       xgettimeofday(struct timeval *tv);
char      *single_argv(char **argv);
char      *strftime_HHMMSS(char *buf, unsigned len, time_t *tp);
char      *strftime_YYYYMMDDHHMMSS(char *buf, unsigned len, time_t *tp);

/* setlocale/LC_TIME stubs — UbixOS doesn't have locale support, so
 * setlocale just returns NULL ("C") and apps that consult the result
 * get the C/POSIX behaviour they'd see in a clean musl. */
#define LC_TIME 2
#define LC_ALL  6
static inline char *setlocale(int category, const char *locale)
{
	(void)category; (void)locale;
	return (char *)"C";
}
int xatoi(const char *numstr);
int xatoi_positive(const char *numstr);
unsigned long xatoul(const char *numstr);
void xstat(const char *fileName, struct stat *statbuf);
int fdprintf(int fd, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int open_or_warn_stdin(const char *filename);
off_t xlseek(int fd, off_t offset, int whence);
void  xwrite(int fd, const void *buf, size_t count);

/* grep / find / recursive walker support */
FILE  *xfopen_stdin(const char *filename);
FILE  *fopen_for_read(const char *filename);
FILE  *fopen_for_write(const char *filename);
char  *xmalloc_fgetline(FILE *fp);
char  *bb_get_chunk_from_file(FILE *file, int *end);
char  bb_process_escape_sequence(const char **ptr);
void   bb_simple_perror_msg_and_die(const char *s) NORETURN;
FILE  *fopen_or_warn(const char *path, const char *mode);
void   bb_putchar_stderr(char ch);
void   bb_error_msg_and_die(const char *fmt, ...) NORETURN __attribute__((format(printf, 1, 2)));
void   llist_add_to(llist_t **old_head, void *data);
void   llist_free(llist_t *elm, void (*freeit)(void *data));

extern int xfunc_error_retval;

struct recursive_state {
	int    depth;
	int    flags;
	void  *userData;
};
enum {
	ACTION_RECURSE        = (1 << 0),
	ACTION_FOLLOWLINKS    = (1 << 1),
	ACTION_FOLLOWLINKS_L0 = (1 << 2),
	ACTION_DEPTHFIRST     = (1 << 3),
	ACTION_REVERSE        = (1 << 4),
	ACTION_QUIET          = (1 << 5),
	ACTION_DANGLING_OK    = (1 << 6),
};
typedef unsigned recurse_flags_t;
enum {
	/* return values for fileAction/dirAction callbacks */
	TRUE_         = 1,
	FALSE_        = 0,
	SKIP          = 2,
};
#define DOT_OR_DOTDOT(s) \
	((s)[0] == '.' && ((s)[1] == '\0' || ((s)[1] == '.' && (s)[2] == '\0')))

typedef int (*recursive_action_fp)(struct recursive_state *state,
                                   const char *fileName,
                                   struct stat *statbuf);
int recursive_action(const char *fileName,
                     unsigned flags,
                     recursive_action_fp fileAction,
                     recursive_action_fp dirAction,
                     void *userData);

/* less / pager support */
int    xopen(const char *pathname, int flags);
char  *xmalloc_ttyname(int fd);
void  *xrealloc_vector(void *vector, unsigned shift, int idx);
int    bb_cat(char **argv);

/* unicode.h provides the ASCII-only inline helpers (isprint_asciionly,
 * unicode_strlen, uni_stat_t, etc.) that several coreutils call without
 * including the header themselves. */
#include "unicode.h"

void die_if_ferror(FILE *fp, const char *fn);

/* coreutils helpers */
typedef unsigned long long uoff_t;
DIR   *warn_opendir(const char *path);
unsigned get_terminal_width(int fd);
char  *bb_mode_string(char buf[11], mode_t mode);

enum {
	VISIBLE_ENDLINE   = 1 << 0,
	VISIBLE_SHOW_TABS = 1 << 1,
};
void visible(unsigned ch, char *buf, int flags);

struct number_state {
	unsigned width;
	unsigned start;
	unsigned inc;
	const char *sep;
	const char *empty_str;
	smallint all, nonempty;
};
int print_numbered_lines(struct number_state *ns, const char *filename);
char  *xmalloc_readlink(const char *path);
char  *xmalloc_readlink_or_warn(const char *path);
const char *make_human_readable_str(unsigned long long val,
                                    unsigned long block_size,
                                    unsigned long display_unit);

/* find: small helpers + argv-max constant */
const char *bb_basename(const char *name);
void   bb_perror_msg_and_die(const char *fmt, ...) NORETURN __attribute__((format(printf, 1, 2)));
extern const char bb_msg_invalid_arg_to[];
extern const char bb_msg_requires_arg[];
extern long bb_arg_max;

/* Fatal signal mask used by bb_signals.  Subset of busybox's set —
 * enough to let pagers restore the terminal before exiting. */
#define BB_FATAL_SIGS \
	((1U << SIGHUP)  | (1U << SIGINT)  | (1U << SIGTERM) | \
	 (1U << SIGPIPE) | (1U << SIGQUIT) | (1U << SIGABRT))
void bb_signals(unsigned sigs, void (*handler)(int));

/* Toggle O_NONBLOCK on an fd.  Used by less to flip stdin between blocking
 * and non-blocking around polled key reads.  Returns previous fl flags. */
int  ndelay_on(int fd);
int  ndelay_off(int fd);
void kill_myself_with_sig(int sig) NORETURN;
int  get_termios_and_make_raw(int fd, struct termios *newterm,
                              struct termios *oldterm, int flags);

/* Additional raw-mode flag accepted by get_termios_and_make_raw —
 * upstream uses it to mean "raw input but keep CRNL translation". */
#define TERMIOS_RAW_CRNL_INPUT  (TERMIOS_RAW_INPUT | TERMIOS_RAW_CRNL)

/* The default tty path used when /dev/tty is the right answer. */
#define CURRENT_TTY  "/dev/tty"
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

/* FILEUTILS_* flags — cp / mv / install / rm / make_directory share this
 * bitmask via the option-parser; bit values must stay stable across the
 * suite (matches upstream busybox include/libbb.h). */
enum {
	FILEUTILS_PRESERVE_STATUS = 1 << 0,
	FILEUTILS_DEREFERENCE     = 1 << 1,
	FILEUTILS_RECUR           = 1 << 2,
	FILEUTILS_FORCE           = 1 << 3,
	FILEUTILS_INTERACTIVE     = 1 << 4,
	FILEUTILS_NO_OVERWRITE    = 1 << 5,
	FILEUTILS_MAKE_HARDLINK   = 1 << 6,
	FILEUTILS_MAKE_SOFTLINK   = 1 << 7,
	FILEUTILS_DEREF_SOFTLINK  = 1 << 8,
	FILEUTILS_DEREFERENCE_L0  = 1 << 9,
	FILEUTILS_VERBOSE         = (1 << 13) * ENABLE_FEATURE_VERBOSE,
	FILEUTILS_UPDATE          = 1 << 14,
	FILEUTILS_NO_TARGET_DIR   = 1 << 15,
	FILEUTILS_TARGET_DIR      = 1 << 16,
	FILEUTILS_CP_OPTBITS      = 18,
	FILEUTILS_RMDEST          = 1 << 19,
	FILEUTILS_REFLINK         = 1 << 20,
	FILEUTILS_REFLINK_ALWAYS  = 1 << 21,
	FILEUTILS_IGNORE_CHMOD_ERR = 1 << 31,
};
#define FILEUTILS_CP_OPTSTR "pdRfinlsLHarPvuTt:"

/* file-ops helpers */
int   is_directory(const char *name, int followLinks);
int   copy_file(const char *source, const char *dest, int flags);
int   remove_file(const char *path, int flags);
int   bb_make_directory(char *path, long mode, int flags);
char *bb_get_last_path_component_strip(char *path);
char *bb_get_last_path_component_nostrip(const char *path);
int    bb_parse_mode(const char *s, unsigned current_mode);
int   bb_ask_y_confirmation(void);
void  bb_simple_error_msg(const char *s);
char *dirname(char *path);
char *last_char_is(const char *s, int c);
int   open_or_warn(const char *pathname, int flags);
int   open3_or_warn(const char *pathname, int flags, int mode);
/* inode/dev hashtable for hardlink preservation — we don't support
 * hardlinks (ENABLE_FEATURE_PRESERVE_HARDLINKS = 0), so these stubs
 * always say "not in table" and the call to add becomes a no-op. */
const char *is_in_ino_dev_hashtable(const struct stat *statbuf);
void  add_to_ino_dev_hashtable(const struct stat *statbuf, const char *name);
void  reset_ino_dev_hashtable(void);
off_t bb_copyfd_eof(int fd1, int fd2);
char *safe_strncpy(char *dst, const char *src, size_t size);
void  xclose(int fd);
char *concat_subpath_file(const char *path, const char *filename);
void   bb_putchar(int c);
void   bb_show_usage(void) NORETURN;
void   bb_simple_error_msg_and_die(const char *s) NORETURN;
unsigned bb_strtou(const char *arg, char **endp, int base);
char  *concat_path_file(const char *path, const char *filename);
char  *skip_whitespace(const char *s);
char  *skip_non_whitespace(const char *s);
int    index_in_strings(const char *strings, const char *key);
void  *llist_pop(llist_t **head);
int    fflush_all(void);
void   tcsetattr_stdin_TCSANOW(const struct termios *tio);
void   set_termios_to_raw(int fd, struct termios *orig_out, int flags);
int    read_key(int fd, char *buffer, int timeout_ms);
int    safe_read_key(int fd, char *buffer, int timeout_ms);
unsigned getopt32(char **argv, const char *applet_opts, ...);
unsigned getopt32long(char **argv, const char *applet_opts, const char *longopts, ...);

extern unsigned option_mask32;

/* Constants used in busybox longopt strings — we ignore the long-opts but
 * still need the symbols to resolve. */
#define No_argument        "\x00"
#define Required_argument  "\x01"
#define Optional_argument  "\x02"

extern const char *applet_name;

#endif /* UBIX_LIBBB_H */
