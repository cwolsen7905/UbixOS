/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ogScrollBar — a reusable vertical scrollbar: draws a track + a proportional
 * thumb and hit-tests a point into thumb-grab / page-up / page-down.  Generic
 * objGFX widget for any scrollable list.  The caller owns the scroll position
 * (`top`) and the click/drag logic; this owns the pixels, the geometry, and the
 * point tests.  Units are caller-defined (rows, cells, pixels — whatever the
 * caller scrolls by); only `total`, `visible`, and `top` need to share a unit.
 */
#ifndef _OBJGFX_OG_SCROLLBAR_H
#define _OBJGFX_OG_SCROLLBAR_H

#include <objgfx/objgfx.h>

class ogScrollBar
{
      public:
	/* Hit-test results. */
	enum
	{
		HIT_NONE = -1,
		HIT_THUMB = 0,   /* press landed on the thumb — start a drag */
		HIT_PAGEUP = 1,  /* track above the thumb — page towards 0 */
		HIT_PAGEDOWN = 2 /* track below the thumb — page towards the end */
	};

	int32 x, y, w, h; /* track rectangle (caller sets each frame) */
	int total;        /* total content units */
	int visible;      /* units that fit in the viewport */
	int top;          /* first visible unit (caller-owned scroll offset) */

	ogScrollBar() : x(0), y(0), w(0), h(0), total(0), visible(0), top(0)
	{
	}

	/* True when the content overflows the viewport (a bar should be shown). */
	bool Needed() const
	{
		return visible > 0 && total > visible;
	}

	/* Largest valid `top` value (0 when everything fits). */
	int MaxTop() const
	{
		int m = total - visible;
		return m > 0 ? m : 0;
	}

	/* Thumb geometry: its top Y (*ty) and height (*th). */
	void Thumb(int32 *ty, int32 *th) const;

	/* Draw the track and thumb (no-op when not Needed()). */
	void Draw(ogSurface &surf) const;

	/* Classify a press at (mx,my): HIT_THUMB / HIT_PAGEUP / HIT_PAGEDOWN, or
	 * HIT_NONE if the point is outside the track. */
	int HitTest(int32 mx, int32 my) const;

	/* Map a thumb drag to a new `top`: @my is the cursor Y, @grabOff the offset
	 * of the cursor within the thumb when the drag began (my - thumbTop). */
	int DragTop(int32 my, int32 grabOff) const;
};

#endif /* _OBJGFX_OG_SCROLLBAR_H */
