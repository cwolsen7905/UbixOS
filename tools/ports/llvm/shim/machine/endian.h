/* machine/endian.h — uBixOS/musl shim for LLVM (its ADT/bit.h includes
 * <machine/endian.h>, a BSD path musl doesn't provide).  Pull in musl's
 * <endian.h> and expose the non-underscore BSD names LLVM expects. */
#ifndef _UBIXOS_MACHINE_ENDIAN_H
#define _UBIXOS_MACHINE_ENDIAN_H
#include <endian.h>
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN __LITTLE_ENDIAN
#endif
#ifndef BIG_ENDIAN
#define BIG_ENDIAN __BIG_ENDIAN
#endif
#ifndef BYTE_ORDER
#define BYTE_ORDER __BYTE_ORDER
#endif
#endif
