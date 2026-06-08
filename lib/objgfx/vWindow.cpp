extern "C" {
#include <stdio.h>
}

#include <stdint.h>
#include <sys/ubix_syscall.h>
#include <objgfx/vWindow.h>

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

/* Legacy SDE command thunk (native syscall 40).  Used only by the old SDE
 * widget path, not the modern views compositor; ported via the portable thunk
 * macro so objgfx links on every arch.  rwAddr is a pointer, so it is passed
 * register-width (uintptr_t) rather than a 32-bit int. */
void vSDECmd(uint32_t command, uintptr_t rwAddr);
}

UBIX_NATIVE_THUNK(vSDECmd, 40);

void vWindow::vSDECommand(uint32_t command) {
  uintptr_t rwAddr = (uintptr_t)realWindow;

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
