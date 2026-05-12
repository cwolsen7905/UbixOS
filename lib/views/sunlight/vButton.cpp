// (C) 2002-2026 The UbixOS Project
#include <vContext.h>
#include <vButton.h>
#include <stdlib.h>
#include <objgfx40.h>
#include <sTypes.h>

vButton::vButton(vContext * parent) : vContext(parent) {
  return;
} // vButton::vButton

bool
vButton::vCreate(void) {
  bool result = false;
  sPixelFormat * pixFmt;
  sSize * borderSize;

  do {
     borderSize = static_cast<sSize *>(vGetStyle("default.button.border.size"));
     if (borderSize == NULL) break;

     pixFmt = static_cast<sPixelFormat *>(vGetStyle("default.desktop.pixelformat"));
     if (pixFmt == NULL) break;

     if (!realView->ogCreate(vGetWidth()+borderSize->size,
                             vGetHeight()+borderSize->size,
                             *pixFmt)) break;

     if (!ogAlias(*realView,
                  borderSize->size,
                  borderSize->size,
                  realView->ogGetMaxX()-borderSize->size,
                  realView->ogGetMaxY()-borderSize->size)) break;

     result = true;
  } while (false);

  return result;
} // vButton::vCreate

vButton::~vButton(void) {
  return;
} // vButton::~vButton
