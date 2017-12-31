#include <objgfx40/vWindow.h>
extern "C" {
  #include <stdio.h>
  #include <unistd.h>
  #include <stdlib.h>
  }

int main() {
  vWindow *window = new vWindow();
  uInt16 i = 0x0;
  uInt16 ii = 0x0;
  uInt16 iii = 0x0;
  if (fork() == 0x0) {
    window->vCreate();
    window->vSDECommand(1);
    while (1) {
      for (i=0x2;i<0xFF;i += 16) {
        for (ii=0x0;ii<0xFF;ii+= 16) {
          for (iii= 0x0;iii< 0xFF;iii+= 16) {
            window->Clear(window->Pack(i,ii,iii));
            window->FillRect(50, 50, 100, 100, window->Pack(255, 0, 0));
            window->FillRect(50, 50, 100, 100, window->Pack(255, 0, 0));
            window->FillRect(100, 50, 150, 100, window->Pack(0, 255, 0));
            window->FillRect(150, 50, 200, 100, window->Pack(0, 0, 255));
            window->FillRect(200, 50, 250, 100, window->Pack(0, 0, 0));
            window->FillRect(250, 50, 300, 100, window->Pack(255, 255, 255));
            window->vSDECommand(3);
            }
          }
        }
      }
    window->vSDECommand(4);    
    }
  return(0);
  }
