/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ogSegmentBar — a reusable horizontal bar split into proportional, individually
 * coloured + labelled segments.  Generic (no app knowledge): it draws a
 * partition-layout bar, a stacked capacity (used/free) bar, or any proportional
 * stacked-bar chart.  Part of objGFX so every app shares it.
 */
#ifndef _OBJGFX_OG_SEGMENT_BAR_H
#define _OBJGFX_OG_SEGMENT_BAR_H

#include <objgfx/objgfx.h>
#include <objgfx/ogScalableFont.h>

/* One segment: a relative weight, a fill colour, and up to two labels.  The
 * `label` is drawn centred inside the segment; the `detail` is drawn centred
 * just below the bar.  A null label/detail is skipped. */
struct ogBarSegment
{
	uInt64 weight;      /* proportional size (sectors/bytes/whatever) */
	uInt32 color;       /* fill colour (0x00RRGGBB) */
	const char *label;  /* in-segment label, or nullptr */
	const char *detail; /* below-bar label, or nullptr */
};

class ogSegmentBar
{
      public:
	/*
	 * Draw a segmented bar filling the rect (x,y)-(x+w-1, y+h-1).  Each segment's
	 * pixel width is proportional to its weight / the total weight.  Segment
	 * labels are rendered only when the segment is wide enough to fit them.  The
	 * caller owns layout (where the bar sits + how much room is left below for
	 * details); detail labels draw at y+h+2.
	 *
	 * @param textInside colour for in-segment labels (default white).
	 * @param textBelow  colour for below-bar detail labels.
	 * @param border     segment divider / outline colour.
	 */
	static void Draw(ogSurface &surf,
	                 ogScalableFont &font,
	                 int32 x,
	                 int32 y,
	                 int32 w,
	                 int32 h,
	                 const ogBarSegment *segs,
	                 int count,
	                 uInt32 border = 0x00202830,
	                 uInt32 textInside = 0x00FFFFFF,
	                 uInt32 textBelow = 0x00303030);
};

#endif /* _OBJGFX_OG_SEGMENT_BAR_H */
