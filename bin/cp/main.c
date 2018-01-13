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

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  int i = 0x0;
  char *buffer = (char *) malloc(0x2000);
  FILE *in = 0x0;
  FILE *out = 0x0;
  in = fopen(argv[1], "rb");
  out = fopen(argv[2], "wb");
  /*
   while (!feof(in)) {
   */
  for (i = 0; i < 21; i++) {
    fread(buffer, 0x1000, 1, in);
    fwrite(buffer, 0x1000, 1, out);
  }
  fclose(in);
  fclose(out);
  return (0x0);
}

/***
 $Log: main.c,v $
 Revision 1.1.1.1  2006/06/01 12:46:09  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:28  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:13:57  reddawg
 no message

 Revision 1.3  2004/05/24 13:40:35  reddawg
 Clean Up


 END
 ***/
