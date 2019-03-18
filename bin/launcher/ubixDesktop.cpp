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
