/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * og_sttimpl.c — the single translation unit that instantiates stb_truetype
 * for objGFX.  stb_truetype.h is public-domain (contrib/stb).  Like the
 * stb_image wrapper, SIMD is irrelevant here (the rasterizer is pure scalar
 * math, executed on the x87 FPU because the tree is built -mno-sse).
 * Assertions are compiled out to avoid pulling in <assert.h>; malloc/free and
 * the libm math functions come from musl.  ogScalableFont.cpp includes
 * stb_truetype.h for the declarations and calls the symbols exported here.
 */

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_assert(x) ((void)0)

#include <stb_truetype.h>
