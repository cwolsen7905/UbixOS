/*
 * Copyright 2026 The UbixOS Project
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

/**
 * \file
 * Single translation unit that instantiates stb_image for the PNG and JPEG
 * content handlers on UbixOS (replacing libpng/libjpeg, which are not ported).
 *
 * STBI_NO_SIMD is mandatory: the world is built -mno-sse, so stb must not emit
 * SSE2 intrinsics.  Only PNG and JPEG are needed here (GIF/BMP are handled by
 * libnsgif/libnsbmp).
 */

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"
