/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ogButton — a reusable labelled push button: draws itself (normal / pressed /
 * disabled) and hit-tests a point.  Generic objGFX widget shared by toolbars,
 * dialogs, etc.  The caller owns the click logic; this owns the pixels + the
 * rectangle test.
 */
#ifndef _OBJGFX_OG_BUTTON_H
#define _OBJGFX_OG_BUTTON_H

#include <objgfx/objgfx.h>
#include <objgfx/ogScalableFont.h>

class ogButton
{
      public:
	int32 x, y, w, h;
	const char *label;
	bool enabled;

	ogButton() : x(0), y(0), w(0), h(0), label(0), enabled(true)
	{
	}

	/* Draw the button.  @pressed inverts the fill for click feedback; a disabled
	 * button is dimmed and ignores @pressed. */
	void Draw(ogSurface &surf, ogScalableFont &font, bool pressed = false) const;

	/* True if (mx,my) is inside the button AND it is enabled. */
	bool Hit(int32 mx, int32 my) const;
};

#endif /* _OBJGFX_OG_BUTTON_H */
