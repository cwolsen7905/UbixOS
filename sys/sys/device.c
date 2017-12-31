/*****************************************************************************************
 Copyright (c) 2002-2005 The UbixOS Project
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

 $Id: device.c 54 2016-01-11 01:29:55Z reddawg $

*****************************************************************************************/

#include <sys/device.h>
#include <ubixos/spinlock.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <assert.h>

/* Linked list of drivers loaded in the system accessable by the subsystem only */
static struct device_node *devices = 0x0;
static struct spinLock deviceSpinLock = SPIN_LOCK_INITIALIZER;

/*****************************************************************************************

 Function: int deviceAdd(int minor,char type,struct device_interface *devInfo);

 Description: This will add a device to the system

 Notes: 

 05/19/2004 - Improving Upon the spec

*****************************************************************************************/
int device_add(int minor,char type,struct device_interface *devInfo) {
  struct device_node *tmpDev = 0x0;
  
  
  tmpDev = (struct device_node *)kmalloc(sizeof(struct device_node));
  if(tmpDev == NULL)
	kprintf("Error Adding Device: memory failure\n");

  tmpDev->prev    = 0x0;
  tmpDev->minor   = minor;
  tmpDev->type    = type;
  tmpDev->devInfo = devInfo;

  spinLock(&deviceSpinLock);
  tmpDev->next    = devices;
  devices         = tmpDev;
  spinUnlock(&deviceSpinLock);

  if (tmpDev->devInfo->initialized == 0x0)
    return(tmpDev->devInfo->init(tmpDev));
  else
    return(0x0);
  }

/*****************************************************************************************

 Function: struct device_node *deviceFind(int major,int minor);

 Description: This will find a device based on major minor

 Notes: 

 05/19/2004 - Improving Upon the spec

*****************************************************************************************/
struct device_node *device_find(int major,int minor) {
  struct device_node *tmpDev = 0x0;
 
  spinLock(&deviceSpinLock);

  for (tmpDev = devices;tmpDev;tmpDev=tmpDev->next) {
    if ((tmpDev->devInfo->major == major) && (tmpDev->minor == minor)) {
      spinUnlock(&deviceSpinLock);
      return(tmpDev);
      }
    }

  spinUnlock(&deviceSpinLock);
  return(0x0);
  }  


/********************************************************************************************

Function: int deviceRemove(struct *device_node);

Description: This will remove a device based on it's pointer

*********************************************************************************************/
int device_remove(struct device_node *deviceToDelete)
{
struct device_node *current, *previous;


current = devices;
previous=NULL;
spinLock(&deviceSpinLock);
               while(current != NULL)
               {
                  if(current==deviceToDelete) break;
                  else
                  {
                     previous = current;
                     current = current->next;
                  }
               }
               if(current == NULL) 
		{
		  spinUnlock(&deviceSpinLock);
		  return 1;
		}
               else
               {
                  if(current == devices) 
			devices = devices->next;
                  else 
			previous->next = current->next;
                  kfree(current);
                  spinUnlock(&deviceSpinLock);
                  return 1;
               }

  spinUnlock(&deviceSpinLock);
return 0x0;
}


/***
 END
 ***/
