/* pconfig.h for the uBixOS oksh (/bin/sh) port.
 *
 * Started from oksh's configure output on the host, then set per **musl**'s
 * feature set (configure can't run target test-programs under -nostdlib).  The
 * BSD-isms macOS has but musl lacks are left undefined so oksh's bundled compat
 * (.c files in OBJS) provide them; the things musl has (incl. setresuid/gid,
 * which macOS lacks) are defined.  See tools/ports/sh/README.md. */
#ifndef PCONFIG_H
#define PCONFIG_H

/* musl provides these (some need _GNU_SOURCE, set in build.sh). */
#define HAVE_ASPRINTF
#define HAVE_CONFSTR
#define HAVE_SIG_T
#define HAVE_ST_MTIM /* native POSIX st_mtim (not BSD st_mtimespec) */
#define HAVE_STRLCAT
#define HAVE_STRLCPY
#define HAVE_TIMERADD
#define HAVE_TIMERCLEAR
#define HAVE_TIMERSUB
#define HAVE_SETRESUID /* musl has it; macOS doesn't */
#define HAVE_SETRESGID

/* musl LACKS these (BSD/OpenBSD-isms) -> oksh's bundled versions are used:
 *   HAVE_CURSES     (no terminfo lib; built -DNO_CURSES anyway)
 *   HAVE_ISSETUGID  -> issetugid.c
 *   HAVE_STRAVIS    -> vis.c        HAVE_STRUNVIS -> unvis.c
 *   HAVE_STRTONUM   -> strtonum.c
 *   HAVE_SIGLIST    -> siglist.c    HAVE_SIGNAME  -> signame.c
 *   HAVE_PLEDGE / HAVE_GETAUXVAL / HAVE_REALLOCARRAY (bundled / stubbed) */

#endif /* PCONFIG_H */
