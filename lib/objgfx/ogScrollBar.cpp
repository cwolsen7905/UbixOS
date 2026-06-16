/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ogScrollBar — reusable vertical scrollbar.  See ogScrollBar.h.
 */
#include <objgfx/ogScrollBar.h>

#define OG_SB_THUMB_MIN 28 /* smallest thumb height, px */

static const uInt32 OG_SB_TRACK = 0x00EEF0F3; /* light track */
static const uInt32 OG_SB_THUMB = 0x006C7480; /* mid-grey thumb */

void ogScrollBar::Thumb(int32 *ty, int32 *th) const
{
	int track = h;
	int t = (total > 0) ? track * visible / total : track;
	if (t < OG_SB_THUMB_MIN)
		t = OG_SB_THUMB_MIN;
	if (t > track)
		t = track;
	int mt = MaxTop();
	*th = t;
	*ty = y + ((mt > 0) ? (track - t) * top / mt : 0);
}

void ogScrollBar::Draw(ogSurface &surf) const
{
	if (!Needed() || w <= 2 || h <= 2)
		return;
	surf.ogFillRect(x, y, x + w - 1, y + h - 1, OG_SB_TRACK);
	int32 ty, th;
	Thumb(&ty, &th);
	surf.ogFillRoundRect(x + 2, ty + 1, x + w - 1, ty + th - 1, 3, OG_SB_THUMB);
}

int ogScrollBar::HitTest(int32 mx, int32 my) const
{
	if (mx < x || mx >= x + w || my < y || my >= y + h)
		return HIT_NONE;
	int32 ty, th;
	Thumb(&ty, &th);
	if (my < ty)
		return HIT_PAGEUP;
	if (my >= ty + th)
		return HIT_PAGEDOWN;
	return HIT_THUMB;
}

int ogScrollBar::DragTop(int32 my, int32 grabOff) const
{
	int32 ty, th;
	Thumb(&ty, &th);
	int span = h - th;
	int mt = MaxTop();
	if (span <= 0 || mt <= 0)
		return 0;
	int rel = (my - grabOff) - y;
	if (rel < 0)
		rel = 0;
	if (rel > span)
		rel = span;
	return rel * mt / span;
}
