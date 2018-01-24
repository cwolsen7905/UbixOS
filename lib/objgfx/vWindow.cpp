extern "C" {
  #include <stdio.h>
}

#include "vWindow.h"

vWindow::vWindow(void) {
  realWindow = new ogSurface();
  titleFont = new ogBitFont();
  return;
} // vWindow::vWindow

bool
vWindow::vCreate(void) {
  if (realWindow->ogCreate(800,600,OG_PIXFMT_24BPP) == false) return false;
  if (ogAlias(*realWindow,                                       // window
              0, 0,                                              // [x1, y1]
              realWindow->ogGetMaxX(), realWindow->ogGetMaxY())  // [x2, y2]
      == false) return false;
  return true;
} // vWindow::vCreate

//void vWindow::vSDECommand(uint32_t command);

extern "C" {
void vSDECmd(uint32_t command, void *realWindow);
asm(
  ".globl vSDECmd\n"
  "vSDECmd:\n"
  "movl $40,%eax\n"
  "int $0x81\n"
  "ret\n"
  );
}


void vWindow::vSDECommand(uint32_t command) {
printf("REAL WINDOW: 0x%X\n", realWindow);
  asm(
      "pushl %%ebx\n"
      "pushl %%ecx\n"
      "int %0\n"
      "add $0x8, %%esp\n"
     :
     : "i" (0x81),"a" (40),"b" (command),"c" (realWindow)
     );
//vSDECmd(command, realWindow);
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
