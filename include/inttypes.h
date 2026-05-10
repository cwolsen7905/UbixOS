#ifndef _INTTYPES_H_
#define _INTTYPES_H_

#include <stdint.h>

/* printf / scanf format macros for stdint types (i386) */
#define PRId8		"d"
#define PRId16		"d"
#define PRId32		"d"
#define PRId64		"lld"
#define PRIu8		"u"
#define PRIu16		"u"
#define PRIu32		"u"
#define PRIu64		"llu"
#define PRIx8		"x"
#define PRIx16		"x"
#define PRIx32		"x"
#define PRIx64		"llx"
#define PRIX8		"X"
#define PRIX16		"X"
#define PRIX32		"X"
#define PRIX64		"llX"
#define PRIi8		"i"
#define PRIi16		"i"
#define PRIi32		"i"
#define PRIi64		"lli"
#define PRIo8		"o"
#define PRIo16		"o"
#define PRIo32		"o"
#define PRIo64		"llo"

#define SCNd8		"hhd"
#define SCNd16		"hd"
#define SCNd32		"d"
#define SCNd64		"lld"
#define SCNu8		"hhu"
#define SCNu16		"hu"
#define SCNu32		"u"
#define SCNu64		"llu"
#define SCNx8		"hhx"
#define SCNx16		"hx"
#define SCNx32		"x"
#define SCNx64		"llx"

#define PRIdPTR		"d"
#define PRIuPTR		"u"
#define PRIxPTR		"x"
#define PRIiPTR		"i"
#define SCNdPTR		"d"
#define SCNuPTR		"u"
#define SCNxPTR		"x"

#endif /* !_INTTYPES_H_ */
