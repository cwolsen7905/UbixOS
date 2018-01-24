/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <objgfx/vWindow.h>
#include <objgfx/ogImage.h>
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
  //if (fork() == 0x0) {
    window->vCreate();
    window->vSDECommand(1);
ogImage * image = new ogImage();
ogSurface * bgImage = new ogSurface();
//image->Load("/ubixos/var/background/ringed800_600.bmp", *bgImage);
image->Load("/ubixos/var/background/sphere800x600.bmp", *bgImage);
window->ogCopy(*bgImage);
            window->vSDECommand(3);
while(1)
  asm("nop");
    while (1) {
      for (i=0x2;i<0xFF;i += 16) {
        for (ii=0x0;ii<0xFF;ii+= 16) {
          for (iii= 0x0;iii< 0xFF;iii+= 16) {
            window->ogClear(window->ogPack(i,ii,iii));
            window->ogFillRect(50, 50, 100, 100, window->ogPack(255, 0, 0));
            window->ogFillRect(50, 50, 100, 100, window->ogPack(255, 0, 0));
            window->ogFillRect(100, 50, 150, 100, window->ogPack(0, 255, 0));
            window->ogFillRect(150, 50, 200, 100, window->ogPack(0, 0, 255));
            window->ogFillRect(200, 50, 250, 100, window->ogPack(0, 0, 0));
            window->ogFillRect(250, 50, 300, 100, window->ogPack(255, 255, 255));
            window->vSDECommand(3);
            }
          }
        }
      }
    window->vSDECommand(4);    
    //}
  return(0);
  }
