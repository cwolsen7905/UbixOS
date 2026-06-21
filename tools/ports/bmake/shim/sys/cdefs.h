/*-
 * sys/cdefs.h — minimal BSD-ism shim for the bmake port.
 *
 * musl (uBixOS's libc) ships no <sys/cdefs.h>, but bmake's make.h includes it
 * unconditionally for __BEGIN_DECLS / __dead / __printflike etc.  This provides
 * just enough.  Part of tools/ports/bmake (see docs/design/third-party-ports-plan.md).
 */
#ifndef _BMAKE_SHIM_SYS_CDEFS_H
#define _BMAKE_SHIM_SYS_CDEFS_H

#ifdef __cplusplus
#define __BEGIN_DECLS extern "C" {
#define __END_DECLS }
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif

#ifndef __P
#define __P(protos) protos
#endif

#ifndef __dead
#define __dead __attribute__((__noreturn__))
#endif
/* NB: do NOT define __unused — musl uses it as a struct-field name (e.g. x86_64
 * bits/stat.h `long __unused[3]`), and bmake uses __attribute__((__unused__))
 * directly, never bare __unused. */
#ifndef __printflike
#define __printflike(fmtarg, firstvararg) __attribute__((__format__(__printf__, fmtarg, firstvararg)))
#endif
#ifndef __predict_true
#define __predict_true(exp) __builtin_expect(((exp) != 0), 1)
#define __predict_false(exp) __builtin_expect(((exp) != 0), 0)
#endif
#ifndef __CONCAT
#define __CONCAT1(x, y) x##y
#define __CONCAT(x, y) __CONCAT1(x, y)
#endif
#ifndef __STRING
#define __STRING(x) #x
#endif

#endif /* _BMAKE_SHIM_SYS_CDEFS_H */
