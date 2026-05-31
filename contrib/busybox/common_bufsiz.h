/*
 * Minimal common_bufsiz.h shim for the UbixOS busybox port.
 * Upstream generates this header at build time to lay out the
 * shared scratch buffer.  We declare bb_common_bufsiz1 and
 * COMMON_BUFSIZE in libbb.h, so this just re-includes that.
 */
#ifndef UBIX_COMMON_BUFSIZ_H
#define UBIX_COMMON_BUFSIZ_H 1
#include "libbb.h"
#endif
