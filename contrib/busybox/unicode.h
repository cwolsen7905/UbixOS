/*
 * Minimal unicode.h shim for the UbixOS busybox port.
 *
 * Mirrors the !ENABLE_UNICODE_SUPPORT branch of upstream busybox's
 * include/unicode.h: ASCII-only operation, every byte is its own char.
 * That's all the applets we currently port (wc, head, tail, vi, ...)
 * actually need on UbixOS today.
 */
#ifndef UBIX_UNICODE_H
#define UBIX_UNICODE_H 1

enum {
	UNICODE_UNKNOWN = 0,
	UNICODE_OFF     = 1,
	UNICODE_ON      = 2,
};

#define unicode_status UNICODE_OFF
#define init_unicode()        ((void)0)
#define reinit_unicode(LANG)  ((void)0)

#define unicode_strlen(s)     strlen(s)
#define unicode_strwidth(s)   strlen(s)

#define unicode_bidi_isrtl(wc)             0
#define unicode_bidi_is_neutral_wchar(wc)  ((wc) <= 126 && !isalpha(wc))

/* busybox uses isprint_asciionly() to mean "isprint() but ASCII-only".
 * On UbixOS where every char is treated as ASCII, isprint suffices. */
#define isprint_asciionly(c)  isprint((unsigned char)(c))

#endif /* UBIX_UNICODE_H */
