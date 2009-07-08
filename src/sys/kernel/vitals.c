/*****************************************************************************************
 Copyright (c) 2002-2004,2009 The UbixOS Project
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are
 permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this list of
 conditions, the following disclaimer and the list of authors.  Redistributions in binary
 form must reproduce the above copyright notice, this list of conditions, the following
 disclaimer and the list of authors in the documentation and/or other materials provided
 with the distribution. Neither the name of the UbixOS Project nor the names of its
 contributors may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 $Id$

*****************************************************************************************/

#include <ubixos/vitals.h>
#include <ubixos/kpanic.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <string.h>

vitalsNode *systemVitals = 0x0;

/************************************************************************

Function: vitals_init();
Description: This will enable the vitals subsystem for ubixos

Notes:

02/20/2004 - Approved Its Quality

************************************************************************/
int vitals_init() {
  /* Initialize Memory For The System Vitals Node */
  systemVitals = (vitalsNode *) kmalloc(sizeof(vitalsNode));

  /* If malloc Failed Then Error */
  if (systemVitals == 0x0) {
    kpanic("Error: kmalloc Failed In initVitals\n");
    }

  /* Set all default values */
  memset(systemVitals,0x0,sizeof(vitalsNode));

  systemVitals->quantum       = 8;
  systemVitals->dQuantum     = 8;

  /* Print Out Info For Vitals: */
  kprintf("vitals0 - Address: [0x%X]\n",systemVitals);

  /* Return so kernel knows that there is no problem */
  return(0x0);
  }

/***
 END
 ***/
