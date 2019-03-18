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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "./include/ubistry.h"

static struct ubistryKey *keys = 0x0;

  
struct ubistryKey * ubistryFindKey(char *name) {
  struct ubistryKey *tmpKey = keys;

  for (;tmpKey;tmpKey=tmpKey->next) {
    if (!strcmp(name,tmpKey->name)) {
      return(tmpKey);
      }
    }
  return(0x0);
  }

int ubistryAddKey(char *name,char *value) {
  struct ubistryKey *tmpKey = (struct ubistryKey *)malloc(sizeof(struct ubistryKey));

  sprintf(tmpKey->name,name);
  sprintf(tmpKey->value,value);

  if (keys == 0x0) {
    keys = tmpKey;
    keys->prev = 0x0;
    keys->next = 0x0;
    } 
  else {
    tmpKey->next = keys;
    tmpKey->prev = 0x0;
    keys->prev   = tmpKey;
    keys         = tmpKey;
    }

  return(0x0);
  }

/***
 $Log: db.c,v $
 Revision 1.1.1.1  2006/06/01 12:46:08  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:28  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:14:04  reddawg
 no message

 Revision 1.2  2004/06/17 15:10:55  reddawg
 Fixed Some Global Variables

 Revision 1.1  2004/05/26 15:41:20  reddawg
 ubistry: added command 666 which will restart the registry server also added
          command 51 to add a key format key,value

 END
 ***/
