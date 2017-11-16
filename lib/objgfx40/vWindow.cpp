extern "C" {
  #include <stdio.h>
  }
#include <objgfx40/vWindow.h>

vWindow::vWindow(void) {
  realWindow = new ogSurface();
  titleFont = new ogBitFont();
  return;
} // vWindow::vWindow

bool
vWindow::vCreate(void) {
  if (realWindow->Create(400,400,OG_PIXFMT_16BPP) == false) return false;
  if (Alias(*realWindow,                                       // window
              0, 0,                                              // [x1, y1]
              realWindow->GetMaxX(), realWindow->GetMaxY())  // [x2, y2]
      == false) return false;
  return true;
} // vWindow::vCreate

void
vWindow::vSDECommand(uInt32 command) {
  asm(
      "int %0"
     :
     : "i" (0x80),"a" (40),"b" (command),"c" (realWindow)
     );
  return;
} // vWindow::vSDECommand

vWindow::~vWindow() {
  delete realWindow;
  delete titleFont;
  return;
} // vWindow::~vWindow

/*
ogSurface -> vWidget  -> vWindow
   |            \------> vButton
   |
   |
   -- ogDisplay_UbixOS -> SDE
*/
