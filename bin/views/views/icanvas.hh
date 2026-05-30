/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* ICanvas — abstract pixel surface                                    */
/*                                                                     */
/* Decouples Window rendering from the concrete Framebuffer so future  */
/* backends (double-buffer, off-screen surface) require no changes to  */
/* Window or any code that draws into a canvas.                        */
/* ------------------------------------------------------------------ */

class ICanvas {
public:
	virtual void     pixel(int x, int y, uint32_t color)            = 0;
	virtual void     rect(int x, int y, int w, int h, uint32_t color) = 0;
	virtual void     blit(int dx, int dy, int w, int h,
	                      const uint32_t *src, int src_stride)       = 0;
	virtual void     ch(int x, int y, char c,
	                    uint32_t fg, uint32_t bg)                    = 0;
	virtual void     text(int x, int y, const char *s,
	                      uint32_t fg, uint32_t bg)                  = 0;
	virtual uint32_t read(int x, int y) const                       = 0;

	virtual ~ICanvas() {}
};
