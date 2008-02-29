/*****************************************************************************************
 Copyright (c) 2002-2004 The UbixOS Project
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

#ifndef _SYSCALLS_H
#define _SYSCALLS_H

#include <ubixos/sched.h>
#include <vfs/file.h>

void sysAuth();
void sysPasswd();
void sysAddModule();
void sysRmModule();
void sysGetpid();
void sysExit();
void sysExec();
int sys_exec();
void sysFork();
void sysCheckPid();
void sysGetFreePage();

void sysFwrite();
void sysFgetc();
void sysFopen();
void sysFread();
void sysFclose();
void sysSchedYield();
void sysFseek();
void sysMkDir();
void sysRmDir();
void sysGetUid();
void sysGetGid();
void sysSetUid();
void sysSetGid();
void sysSDE();
void sysGetDrives();
void sysGetCwd();
void sysChDir();
void sysGetUptime();
void sysGetTime();
void sysStartSDE();
void sysUnlink();
void sysMpiCreateMbox();
void sysMpiDestroyMbox();
void sysMpiPostMessage();
void sysMpiFetchMessage();
void sysMpiSpam();

typedef void (*functionPTR)();

functionPTR systemCalls[] = {
  invalidCall,      /**   0 **/
  sysGetpid,        /**   1 **/
  sysExit,          /**   2 **/
  sysExec,          /**   3 **/
  sysFork,          /**   4 **/
  sysFgetc,         /**   5 **/
  sysCheckPid,      /**   6 **/
  sysGetFreePage,   /**   7 **/
  sysFopen,         /**   8 **/
  invalidCall,      /**   9 **/
  sysFclose,        /**  10 **/
  sysSchedYield,    /**  11 **/
  invalidCall,      /**  12 **/
  invalidCall,      /**  13 **/
  invalidCall,      /**  14 **/
  sys_exec,      /**  15 **/
  invalidCall,      /**  16 **/
  invalidCall,      /**  17 **/
  invalidCall,      /**  18 **/
  invalidCall,      /**  19 **/
  sysFopen,         /**  20 Opens A File Node       **/
  sysFclose,        /**  21 Closes A File Node      **/
  sysFread,         /**  22 File Read               **/
  sysFwrite,        /**  23 File Write              **/
  sysMkDir,         /**  24 Make Directory          **/
  sysRmDir,         /**  25 Remove Directory        **/
  sysGetCwd,        /**  26 Get Current Working Dir **/
  sysFseek,         /**  27 Set FD Position         **/
  sysChDir,         /**  28 Change Dir              **/
  sysMkDir,         /**  29 Create Directory        **/
  sysUnlink,        /**  30 Unlink                  **/
  sysGetUid,        /**  31 Get User Id             **/
  sysGetGid,        /**  32 Get Group Id            **/
  sysSetUid,        /**  33 Set User Id             **/
  sysSetGid,        /**  34 Set Group Id            **/
  sysAuth,    	    /**  35 Authenticates the user  **/
  sysPasswd,        /**  36 Change user password    **/
  sysAddModule,     /**  37 Add Kernel Module       **/
  sysRmModule,      /**  38 Remove Kernel Module    **/
  invalidCall,      /**  39 **/
  //sysSDE,           /**  40 SDE Kernel Interface    **/
  invalidCall,      /**  40 **/
  invalidCall,      /**  41 **/
  invalidCall,      /**  42 **/
  invalidCall,      /**  43 **/
  invalidCall,      /**  44 **/
  sysGetDrives,     /**  45 Get Drives              **/      
  sysGetUptime,     /**  46 Get Uptime **/
  sysGetTime,       /**  47 Get Time   **/
  sysStartSDE,      /**  48 start SDE **/
  invalidCall,      /**  49 **/
  sysMpiCreateMbox,   /**  50 mpiCreateMbox           **/
  sysMpiDestroyMbox,  /**  51 mpiDestroyMbox          **/
  sysMpiPostMessage,  /**  52 mpiPostMessage          **/
  sysMpiFetchMessage, /**  53 mpiFetchMessage         **/
  sysMpiSpam,         /**  54 mpiSpam                 **/
  };

int totalCalls = sizeof(systemCalls)/sizeof(functionPTR);

#endif

/***
 $Log$
 Revision 1.1.1.1  2007/01/17 03:31:52  reddawg
 UbixOS

 Revision 1.3  2006/12/19 14:12:54  reddawg
 rtld-elf almost workign

 Revision 1.2  2006/10/12 17:05:44  reddawg
 Removing SDE

 Revision 1.1.1.1  2006/06/01 12:46:14  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:56  reddawg
 no message

 Revision 1.5  2005/08/04 22:48:39  fsdfs

 added 4 new syscalls: sysAuth(), sysPasswd(), sysAddModule(), sysRmModule()

 Revision 1.4  2004/05/26 15:39:22  reddawg
 mpi: brought mpiDestroyMbox(char *name) in to the userland

 Revision 1.3  2004/05/25 15:42:19  reddawg
 Enabled mpiSpam();

 Revision 1.2  2004/05/21 15:20:00  reddawg
 Cleaned up


 END
 ***/
