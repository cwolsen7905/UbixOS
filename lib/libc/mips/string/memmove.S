/*	$NetBSD: memmove.S,v 1.1 2005/12/20 19:28:50 christos Exp $	*/

#include <machine/asm.h>
__FBSDID("$FreeBSD: releng/11.1/lib/libc/mips/string/memmove.S 178580 2008-04-26 12:08:02Z imp $");

#define MEMMOVE
#include "bcopy.S"
