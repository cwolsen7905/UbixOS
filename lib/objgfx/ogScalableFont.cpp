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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stb_truetype.h>

#include "objgfx/ogScalableFont.h"

/* Blend an 8-bit coverage value between two channel bytes. */
static inline uInt8 blend_channel(uInt8 base, uInt8 top, uInt8 cov)
{
	return (uInt8)(base + (((int)top - (int)base) * (int)cov) / 255);
}

ogScalableFont::ogScalableFont()
    : ttf_(nullptr), ttf_data_(nullptr), scale_(0.0f), px_height_(0), ascent_(0), descent_(0), line_height_(0)
{
	fg_.red = fg_.green = fg_.blue = 255;
	fg_.alpha = 255;
	bg_.red = bg_.green = bg_.blue = 0;
	bg_.alpha = 0; /* default: transparent background (blend over destination) */
	memset(cache_, 0, sizeof(cache_));
}

ogScalableFont::~ogScalableFont()
{
	for (int i = 0; i < OG_SF_NUM_CP; i++)
		if (cache_[i].cov)
			stbtt_FreeBitmap(cache_[i].cov, nullptr);
	free(ttf_);
	free(ttf_data_);
}

bool ogScalableFont::Load(const char *fontFile, int pixelHeight)
{
	FILE *f = fopen(fontFile, "rb");
	if (!f)
		return false;

	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz <= 0)
	{
		fclose(f);
		return false;
	}

	uInt8 *data = (uInt8 *)malloc((size_t)sz);
	if (!data)
	{
		fclose(f);
		return false;
	}
	if (fread(data, 1, (size_t)sz, f) != (size_t)sz)
	{
		free(data);
		fclose(f);
		return false;
	}
	fclose(f);

	stbtt_fontinfo *fi = (stbtt_fontinfo *)malloc(sizeof(stbtt_fontinfo));
	if (!fi || !stbtt_InitFont(fi, data, stbtt_GetFontOffsetForIndex(data, 0)))
	{
		free(fi);
		free(data);
		return false;
	}

	ttf_data_ = data;
	ttf_ = fi;
	px_height_ = pixelHeight;
	scale_ = stbtt_ScaleForPixelHeight(fi, (float)pixelHeight);

	int asc, desc, gap;
	stbtt_GetFontVMetrics(fi, &asc, &desc, &gap);
	ascent_ = (int)(asc * scale_ + 0.5f);
	descent_ = (int)(-desc * scale_ + 0.5f);
	line_height_ = (int)((asc - desc + gap) * scale_ + 0.5f);
	return true;
}

const ogScalableFont::Glyph *ogScalableFont::GetGlyph(int cp)
{
	if (cp < OG_SF_FIRST_CP || cp > OG_SF_LAST_CP || !ttf_)
		return nullptr;

	Glyph *g = &cache_[cp - OG_SF_FIRST_CP];
	if (g->ready)
		return g;

	stbtt_fontinfo *fi = (stbtt_fontinfo *)ttf_;
	int adv, lsb;
	stbtt_GetCodepointHMetrics(fi, cp, &adv, &lsb);
	g->advance = (int)(adv * scale_ + 0.5f);

	/* Rasterize the antialiased coverage bitmap (NULL/zero-size for blanks). */
	g->cov = stbtt_GetCodepointBitmap(fi, scale_, scale_, cp, &g->w, &g->h, &g->xoff, &g->yoff);
	g->ready = true;
	return g;
}

int ogScalableFont::Advance(int cp)
{
	const Glyph *g = GetGlyph(cp);
	return g ? g->advance : 0;
}

uInt32 ogScalableFont::TextWidth(const char *s)
{
	if (!s)
		return 0;
	float pen = 0.0f;
	for (const unsigned char *p = (const unsigned char *)s; *p; p++)
		pen += (float)Advance(*p);
	return (uInt32)(pen + 0.5f);
}

uInt32 ogScalableFont::TextHeight(const char *)
{
	return (uInt32)line_height_;
}

void ogScalableFont::SetFGColor(uInt32 r, uInt32 g, uInt32 b, uInt32 a)
{
	fg_.red = (uInt8)r;
	fg_.green = (uInt8)g;
	fg_.blue = (uInt8)b;
	fg_.alpha = (uInt8)a;
}

void ogScalableFont::SetBGColor(uInt32 r, uInt32 g, uInt32 b, uInt32 a)
{
	bg_.red = (uInt8)r;
	bg_.green = (uInt8)g;
	bg_.blue = (uInt8)b;
	bg_.alpha = (uInt8)a;
}

void ogScalableFont::PutChar(ogSurface &dest, int32 x, int32 y, char ch)
{
	const Glyph *g = GetGlyph((unsigned char)ch);
	if (!g || !dest.ogAvail())
		return;

	/* Defensive: a cached glyph whose metrics are implausible (corrupted cache
	 * entry) must never drive the blit loop below into a wild read.  Real glyphs
	 * at UI sizes are a few tens of pixels; anything far larger, a negative
	 * dimension, or a non-empty glyph with a NULL coverage bitmap is rejected. */
	if (g->advance < 0 || g->advance > 4096 || g->w < 0 || g->w > 4096 || g->h < 0 || g->h > 4096 ||
	    (g->h > 0 && g->cov == nullptr))
		return;

	int baseline = y + ascent_;
	bool opaque_bg = bg_.alpha != 0;

	/* In opaque mode fill the whole character cell (advance wide, one cap+
	 * descent band tall) with the background first, so redrawing text cleanly
	 * erases what was there — matching ogBitFont's per-cell fill.  The band
	 * height equals the requested pixel height (ScaleForPixelHeight makes
	 * ascent-descent == pixelHeight), so adjacent rows do not overlap. */
	if (opaque_bg)
	{
		int cell_h = ascent_ + descent_;
		for (int cy = 0; cy < cell_h; cy++)
			for (int cx = 0; cx < g->advance; cx++)
				dest.ogSetPixel(x + cx, y + cy, bg_.red, bg_.green, bg_.blue, bg_.alpha);
	}

	for (int gy = 0; gy < g->h; gy++)
	{
		int py = baseline + g->yoff + gy;
		const uInt8 *crow = g->cov + (size_t)gy * g->w;
		for (int gx = 0; gx < g->w; gx++)
		{
			uInt8 cov = crow[gx];
			if (cov == 0)
				continue; /* background already painted (opaque) or left untouched */
			int px = x + g->xoff + gx;

			uInt8 br, bgc, bb;
			if (opaque_bg)
			{
				br = bg_.red;
				bgc = bg_.green;
				bb = bg_.blue;
			}
			else
			{
				/* Blend over whatever is already on the surface. */
				dest.ogUnpack(dest.ogGetPixel(px, py), br, bgc, bb);
			}
			dest.ogSetPixel(px,
			                py,
			                blend_channel(br, fg_.red, cov),
			                blend_channel(bgc, fg_.green, cov),
			                blend_channel(bb, fg_.blue, cov),
			                255);
		}
	}
}

void ogScalableFont::PutString(ogSurface &dest, int32 x, int32 y, const char *s)
{
	if (!s || !dest.ogAvail())
		return;

	float pen = (float)x;
	for (const unsigned char *p = (const unsigned char *)s; *p; p++)
	{
		PutChar(dest, (int32)(pen + 0.5f), y, (char)*p);
		pen += (float)Advance(*p);
	}
}

void ogScalableFont::CenterTextX(ogSurface &dest, int32 y, const char *s)
{
	int32 x = (int32)((dest.ogGetMaxX() + 1 - (int)TextWidth(s)) / 2);
	if (x < 0)
		x = 0;
	PutString(dest, x, y, s);
}
