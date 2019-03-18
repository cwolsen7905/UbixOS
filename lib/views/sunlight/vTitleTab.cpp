#include <string>
#include <vContext.h>
#include <vTitleTab.h>
#include <sTypes.h>

vTitleTab::vTitleTab(vContext * parent) : vContext(parent) {
  // Allocate a new font
  font = new ogBitFont();

  // Set the title to nothing
  title = "";

  // Retrieve the default font filename out of the style tree
  sString * fontFileName = dynamic_cast<sString *>(vGetStyle("default.font.filename"));

  // Attempt to load the font
  if (fontFileName != NULL) {
    // I should check for failure here, although everything fails quietly...
    // so even if it does fail it won't matter much
    font->Load(fontFileName->c_str(), 0);
  }

  sRGBA8Color * color = dynamic_cast<sRGBA8Color *>(vGetStyle("default.font.color.background"));
  if (NULL != color) 
    font->SetBGColor(color->red, color->blue, color->green, color->alpha);

  color = dynamic_cast<sRGBA8Color *>(vGetStyle("default.font.color.foreground"));
  if (color != NULL) 
    font->SetFGColor(color->red, color->blue, color->green, color->alpha);

  return;
} // vTitleTab::vTitleTab

void
vTitleTab::vDraw(void) {
  ogPoint2d points[4];
  sBGColor * BGColor = dynamic_cast<sBGColor *>(vGetStyle("default.title.color.passive"));
  if (BGColor == NULL) return;
 
  points[0].x = points[0].y = points[1].y = points[3].x = 0;
  points[1].x = points[2].x = ogGetMaxX()+1;
  points[2].y = points[3].y = ogGetMaxY();

  ogFillGouraudPolygon(4, points, BGColor->colors);
  font->JustifyText(*this, centerText, centerText, title.c_str());
  return;
} // vTitleTab::vDraw()

void
vTitleTab::vSetTitle(const std::string newTitle) {
  title = newTitle;
  return;
} // vTitleTab::vSetTitle

vTitleTab::~vTitleTab(void) {
  delete font;
  font = NULL;
  return;
} // vTitleTab::~vTitleTab
