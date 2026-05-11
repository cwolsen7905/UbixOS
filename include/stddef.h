/*
 * Copyright (c) 2002-2026 The UbixOS Project. All rights reserved.
 * Minimal stddef.h — no FreeBSD header dependencies so TCC can include it.
 */

#ifndef _STDDEF_H_
#define _STDDEF_H_

#ifdef __TINYC__
/* TCC provides these via compiler built-ins */
typedef unsigned int  size_t;
typedef int           ptrdiff_t;
typedef int           intptr_t;
typedef unsigned int  uintptr_t;
#ifndef __int8_t_defined
#define __int8_t_defined
typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
#endif
#else
/* GCC / Clang cross-compiler */
typedef __SIZE_TYPE__    size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __PTRDIFF_TYPE__ intptr_t;
typedef __SIZE_TYPE__    uintptr_t;
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#define offsetof(type, member) ((size_t)&((type *)0)->member)

#endif /* _STDDEF_H_ */
