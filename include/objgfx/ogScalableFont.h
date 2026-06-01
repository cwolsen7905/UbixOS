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

#ifndef OGSCALABLEFONT_H
#define OGSCALABLEFONT_H

#include "objgfx.h"

/*
 * ogScalableFont — a TrueType (.ttf) font rasterized at a fixed pixel height
 * with antialiasing, via the vendored stb_truetype.  Replaces the fixed-size
 * bitmap ogBitFont/.DPF path so text scales cleanly with the display
 * resolution.  Glyphs are rasterized lazily into an 8-bit coverage cache the
 * first time each codepoint is drawn; only Latin-1 (32..255) is cached, which
 * covers all current UI text.
 *
 * The cache is a flat array (not std::map) on purpose: libobjgfx links only
 * -lc, so the font core must avoid the C++ standard library.
 */

#define OG_SF_FIRST_CP 32 /* first cached codepoint (space) */
#define OG_SF_LAST_CP 255 /* last cached codepoint (Latin-1) */
#define OG_SF_NUM_CP (OG_SF_LAST_CP - OG_SF_FIRST_CP + 1)

class ogScalableFont
{
      public:
	/* A single rasterized glyph: an 8-bit coverage bitmap plus placement. */
	struct Glyph
	{
		uInt8 *cov;  /* w*h coverage, 0..255; NULL until rasterized */
		int w, h;    /* coverage bitmap dimensions in pixels */
		int xoff;    /* left bearing from the pen position */
		int yoff;    /* top offset from the baseline (negative = above) */
		int advance; /* horizontal pen advance in pixels */
		bool ready;  /* this slot has been resolved (cov may still be NULL) */
	};

	ogScalableFont();
	~ogScalableFont();

	/**
	 * Load a TrueType font and rasterize at the given pixel height.
	 *
	 * @param fontFile    path to a .ttf file.
	 * @param pixelHeight cap-to-baseline target height in pixels.
	 * @return true on success; false if the file is missing or not a TTF.
	 */
	bool Load(const char *fontFile, int pixelHeight);

	bool IsValid() const
	{
		return ttf_ != nullptr;
	}

	int Ascent() const
	{
		return ascent_;
	}
	int Descent() const
	{
		return descent_;
	} /* positive magnitude below baseline */
	int LineHeight() const
	{
		return line_height_;
	}
	int PixelHeight() const
	{
		return px_height_;
	}

	/** Horizontal advance of one codepoint, in whole pixels. */
	int Advance(int cp);

	/** Total advance width of a string, in pixels. */
	uInt32 TextWidth(const char *s);

	/** Cap/line height of the font, in pixels (for vertical layout). */
	uInt32 TextHeight(const char *s);

	/* Colours used when blending coverage onto a surface.  If the background
	 * alpha is 0 the glyph is alpha-blended over whatever is already on the
	 * surface; otherwise the cell is filled by lerping BG -> FG. */
	void SetFGColor(uInt32 r, uInt32 g, uInt32 b, uInt32 a = 255);
	void SetBGColor(uInt32 r, uInt32 g, uInt32 b, uInt32 a = 255);

	/* Draw, treating (x, y) as the top-left of the text box (the baseline is
	 * placed at y + Ascent()), matching ogBitFont's PutChar/PutString. */
	void PutChar(ogSurface &dest, int32 x, int32 y, char ch);
	void PutString(ogSurface &dest, int32 x, int32 y, const char *s);
	void CenterTextX(ogSurface &dest, int32 y, const char *s);

	/**
	 * Resolve (rasterizing on first use) the glyph for a codepoint.
	 *
	 * Exposed so non-ogSurface consumers (e.g. the views compositor blending
	 * onto its 32bpp shadow) can do their own coverage blit.
	 *
	 * @return cached glyph, or NULL if the codepoint is outside 32..255.
	 */
	const Glyph *GetGlyph(int cp);

      private:
	void *ttf_;       /* stbtt_fontinfo (opaque to avoid leaking stb into the header) */
	uInt8 *ttf_data_; /* the font file kept resident; fontinfo points into it */
	float scale_;     /* stbtt scale for px_height_ */
	int px_height_;   /* requested pixel height */
	int ascent_;      /* scaled, pixels (positive) */
	int descent_;     /* scaled, positive magnitude */
	int line_height_; /* ascent + descent + line gap */
	ogRGBA8 fg_, bg_;
	Glyph cache_[OG_SF_NUM_CP];

	ogScalableFont(const ogScalableFont &);            /* non-copyable */
	ogScalableFont &operator=(const ogScalableFont &); /* non-copyable */
};

#endif /* OGSCALABLEFONT_H */
