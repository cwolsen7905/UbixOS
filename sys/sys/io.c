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

#include <sys/io.h>

/*!
 * \brief input one byte from specified port
 *
 * \param port address of port for reading
 * \return returns unsigned char value
 *
 */
unsigned char inportByte(unsigned int port) {
  unsigned char retVal;
  asm volatile(
    "inb %%dx,%%al"
    : "=a" (retVal)
    : "d" (port)
  );
  return (retVal);
}

/*!
 * \brief input one word from specified port
 *
 * \param port address of port for reading
 * \return returns unsigned short value
 *
 */
unsigned short inportWord(unsigned int port) {
  unsigned short retVal;
  asm volatile(
    "inw %%dx,%%ax"
    : "=a" (retVal)
    : "d" (port)
  );
  return (retVal);
}

/*!
 * \brief outputut one byte to specified port
 *
 * \param port address of port for writing
 * \param value byte to output to port
 *
 */
void outportByte(unsigned int port, unsigned char value) {
  asm volatile(
    "outb %%al,%%dx"
    :
    : "d" (port), "a" (value)
  );
}

/*!
 * \brief outputut one byte to specified port with a delay
 *
 * \param port address of port for writing
 * \param value byte to output to port
 *
 */
void outportByteP(unsigned int port, unsigned char value) {
  asm volatile(
    "outb %%al,%%dx\n"
    "outb %%al,$0x80\n"
    :
    : "d" (port), "a" (value)
  );
}

/*!
 * \brief outputut one word to specified port
 *
 * \param port address of port for writing
 * \param value word to output to port
 *
 */
void outportWord(unsigned int port, unsigned short value) {
  asm volatile(
    "outw %%ax,%%dx"
    :
    : "d" (port), "a" (value)
  );
}

/*!
 * \brief outputut one double word to specified port
 *
 * \param port address of port for writing
 * \param value double word to output to port
 *
 */
void outportDWord(unsigned int port, unsigned long value) {
  asm volatile(
    "outl %%eax,%%dx"
    :
    : "d" (port), "a" (value)
  );
}

/*!
 * \brief input one double word from specified port
 *
 * \param port address of port for reading
 * \return returns unsigned double word value
 *
 */
unsigned long inportDWord(unsigned int port) {
  unsigned long retVal;
  asm volatile(
    "inl %%dx,%%eax"
    : "=a" (retVal)
    : "d" (port)
  );
  return (retVal);
}

/***
 END
 ***/
