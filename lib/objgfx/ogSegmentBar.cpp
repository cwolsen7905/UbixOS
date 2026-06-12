/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ogSegmentBar — reusable proportional segmented bar.  See ogSegmentBar.h.
 */
#include <objgfx/ogSegmentBar.h>

/* Pull the 8-bit channels out of a 0x00RRGGBB colour for ogScalableFont. */
static inline uInt32 og_r(uInt32 c)
{
	return (c >> 16) & 0xFF;
}
static inline uInt32 og_g(uInt32 c)
{
	return (c >> 8) & 0xFF;
}
static inline uInt32 og_b(uInt32 c)
{
	return c & 0xFF;
}

void ogSegmentBar::Draw(ogSurface &surf,
                        ogScalableFont &font,
                        int32 x,
                        int32 y,
                        int32 w,
                        int32 h,
                        const ogBarSegment *segs,
                        int count,
                        uInt32 border,
                        uInt32 textInside,
                        uInt32 textBelow)
{
	if (segs == 0 || count <= 0 || w <= 1 || h <= 1)
		return;

	uInt64 total = 0;
	for (int i = 0; i < count; i++)
		total += segs[i].weight;
	if (total == 0)
		total = 1;

	int32 cursor = x;
	for (int i = 0; i < count; i++)
	{
		/* Last segment soaks up any rounding remainder so the bar fills w. */
		int32 segw = (i == count - 1) ? (x + w - cursor) : (int32)((uInt64)w * segs[i].weight / total);
		if (segw < 1)
			segw = 1;
		int32 x2 = cursor + segw - 1;
		if (x2 > x + w - 1)
			x2 = x + w - 1;

		surf.ogFillRect(cursor, y, x2, y + h - 1, segs[i].color);

		/* In-segment label, centred, if it fits. */
		if (segs[i].label != 0)
		{
			uInt32 tw = font.TextWidth(segs[i].label);
			if ((int32)tw + 8 <= segw)
			{
				font.SetFGColor(og_r(textInside), og_g(textInside), og_b(textInside));
				int32 tx = cursor + (segw - (int32)tw) / 2;
				int32 ty = y + (h - (int32)font.GetHeight()) / 2;
				font.PutString(surf, tx, ty, segs[i].label);
			}
		}

		/* Below-bar detail, centred under the segment, if it fits. */
		if (segs[i].detail != 0)
		{
			uInt32 tw = font.TextWidth(segs[i].detail);
			if ((int32)tw <= segw + 8)
			{
				font.SetFGColor(og_r(textBelow), og_g(textBelow), og_b(textBelow));
				int32 tx = cursor + (segw - (int32)tw) / 2;
				if (tx < x)
					tx = x;
				font.PutString(surf, tx, y + h + 3, segs[i].detail);
			}
		}

		/* Divider between this segment and the next. */
		if (i < count - 1)
			surf.ogVLine(x2, y, y + h - 1, border);
		cursor += segw;
	}

	/* Outline the whole bar. */
	surf.ogRect(x, y, x + w - 1, y + h - 1, border);
}
