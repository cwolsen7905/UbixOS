/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ogButton — reusable labelled push button.  See ogButton.h.
 */
#include <objgfx/ogButton.h>

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

/* Default: the original light-grey square buttons (apps may restyle). */
ogButtonStyle ogButton::style = {
    0x00ECEFF3, 0x00C8CED6, 0x00E4E6E8, 0x009098A4, 0x009098A4, 0x00C0C4C8, 0x00202830, 0x00A0A4A8, 0};

void ogButton::Draw(ogSurface &surf, ogScalableFont &font, bool pressed) const
{
	if (w <= 1 || h <= 1)
		return;

	const ogButtonStyle &s = style;
	uInt32 face = enabled ? (pressed ? s.face_pressed : s.face) : s.face_disabled;
	uInt32 edge = enabled ? (pressed ? s.edge_pressed : s.edge) : s.edge_disabled;
	uInt32 text = enabled ? s.text : s.text_disabled;

	if (s.radius > 0)
	{
		surf.ogFillRoundRect(x, y, x + w - 1, y + h - 1, s.radius, face);
		surf.ogRoundRect(x, y, x + w - 1, y + h - 1, s.radius, edge);
	}
	else
	{
		surf.ogFillRect(x, y, x + w - 1, y + h - 1, face);
		surf.ogRect(x, y, x + w - 1, y + h - 1, edge);
	}

	if (label != 0)
	{
		uInt32 tw = font.TextWidth(label);
		int32 tx = x + (w - (int32)tw) / 2;
		int32 ty = y + (h - (int32)font.GetHeight()) / 2;
		font.SetFGColor(og_r(text), og_g(text), og_b(text));
		font.PutString(surf, tx, ty, label);
	}
}

bool ogButton::Hit(int32 mx, int32 my) const
{
	return (enabled && mx >= x && mx < x + w && my >= y && my < y + h);
}
