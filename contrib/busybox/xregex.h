/*
 * xregex.h shim for the UbixOS busybox port.  Just brings in <regex.h>
 * (musl provides POSIX regex) and declares the busybox xregcomp wrapper
 * implemented in libbb_stubs.c.
 */
#ifndef UBIX_XREGEX_H
#define UBIX_XREGEX_H 1

#include <regex.h>

#define PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN
#define POP_SAVED_FUNCTION_VISIBILITY

char *regcomp_or_errmsg(regex_t *preg, const char *regex, int cflags);
void  xregcomp(regex_t *preg, const char *regex, int cflags);

#endif /* UBIX_XREGEX_H */
