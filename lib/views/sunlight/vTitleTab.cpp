// (C) 2002-2026 The UbixOS Project
#include <string.h>
#include <vContext.h>
#include <vTitleTab.h>
#include <sTypes.h>

vTitleTab::vTitleTab(vContext * parent) : vContext(parent) {
  font     = new ogBitFont();
  title[0] = '\0';

  sString * fontFileName = static_cast<sString *>(vGetStyle("default.font.filename"));
  if (fontFileName != NULL)
    font->Load(fontFileName->c_str(), 0);

  sRGBA8Color * color = static_cast<sRGBA8Color *>(vGetStyle("default.font.color.background"));
  if (color != NULL)
    font->SetBGColor(color->red, color->blue, color->green, color->alpha);

  color = static_cast<sRGBA8Color *>(vGetStyle("default.font.color.foreground"));
  if (color != NULL)
    font->SetFGColor(color->red, color->blue, color->green, color->alpha);

  return;
} // vTitleTab::vTitleTab

void
vTitleTab::vDraw(void) {
  ogPoint2d points[4];
  sBGColor * BGColor = static_cast<sBGColor *>(vGetStyle("default.title.color.passive"));
  if (BGColor == NULL) return;

  points[0].x = points[0].y = points[1].y = points[3].x = 0;
  points[1].x = points[2].x = ogGetMaxX()+1;
  points[2].y = points[3].y = ogGetMaxY();

  ogFillGouraudPolygon(4, points, BGColor->colors);
  font->JustifyText(*this, centerText, centerText, title);
  return;
} // vTitleTab::vDraw()

void
vTitleTab::vSetTitle(const char * newTitle) {
  strncpy(title, newTitle, 255);
  title[255] = '\0';
  return;
} // vTitleTab::vSetTitle

vTitleTab::~vTitleTab(void) {
  delete font;
  font = NULL;
  return;
} // vTitleTab::~vTitleTab
