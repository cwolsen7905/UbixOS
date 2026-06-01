/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * og_stbimpl.c — the single translation unit that instantiates stb_image's PNG
 * decoder for objGFX.  stb_image.h is public-domain (contrib/stb); only the PNG
 * path is compiled in.  SIMD is disabled because the whole tree is built
 * -mno-sse (executing XMM instructions faults the kernel); STDIO/HDR/LINEAR are
 * disabled to avoid pulling in FILE* and <math.h>.  ogImage::Load() calls the
 * exported stbi_load_from_memory().
 */

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_NO_FAILURE_STRINGS
#define STBI_ASSERT(x) ((void)0)

#include <stb_image.h>
