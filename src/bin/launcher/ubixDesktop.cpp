#include <vContext.h>
#include <ubixDesktop.h>
#include <sStyle.h>
#include <sTypes.h>

ubixDesktop::ubixDesktop(vContext * parent) : vContext(parent) {
  vSetStyle("default.button.border.size", new sSize(2));
  vSetStyle("default.font.filename", new sString("ROM8X14.DPF"));
  vSetStyle("default.font.color.foreground", 
            new sRGBA8Color(255, 255, 255, 255));
  vSetStyle("default.font.color.background", 
            new sRGBA8Color(0, 0, 0, 255));
  vSetStyle("default.desktop.pixelformat", 
            new sPixelFormat(16, 11,5,0,0,  5,6,5,0));
  return;
} // ubixDesktop::ubixDesktop

bool
ubixDesktop::vCreate(void) {
  return true;
} // ubixDesktop::vCreate

void
ubixDesktop::vDraw(void) {
  return;
} // ubixDesktop::vDraw

ubixDesktop::~ubixDesktop(void) {
  return;
}
