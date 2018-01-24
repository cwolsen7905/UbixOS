extern "C" {
  #include <stdio.h>
}

#include "vWindow.h"

vWindow::vWindow(void) {
  realWindow = new ogSurface();
  titleFont = new ogBitFont();
  return;
} // vWindow::vWindow

bool vWindow::vCreate(void) {
  if (realWindow->ogCreate(800,600,OG_PIXFMT_24BPP) == false) {
    return false;
  }

  if (ogAlias(*realWindow, 0, 0, realWindow->ogGetMaxX(), realWindow->ogGetMaxY()) == false) {
    return false;
  }

  return true;
} // vWindow::vCreate

extern "C" {

void vSDECmd(uint32_t command, uint32_t rwAddr);

asm(
  ".globl vSDECmd\n"
  "vSDECmd:\n"
  "movl $40,%eax\n"
  "int $0x81\n"
  "ret\n"
  );

}

void vWindow::vSDECommand(uint32_t command) {
  uint32_t rwAddr = (uint32_t)realWindow;
  printf("\nREAL WINDOW: 0x%X:0x%X:0x%X\n", &realWindow, realWindow, rwAddr);

  /*
  asm(
    "pushl %%ebx\n"
    "pushl %%ecx\n"
    "int %0\n"
    "add $0x8, %%esp\n"
    :
    : "i" (0x81),"a" (40),"b" (command),"c" (rwAddr)
  );
  */

  vSDECmd(command, rwAddr);

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
