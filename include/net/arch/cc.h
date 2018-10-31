#ifndef __ARCH_CC_H__
#define __ARCH_CC_H__

#include <sys/ubixos/time.h>

#define PACK_STRUCT_FIELD(x) x __attribute__((packed))
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

#define LWIP_ERR_T int
/*
#ifdef LWIP_ERR_T
typedef uint8_t err_t;
#endif
*/

#endif
