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

 $Id: libcpp.cc 54 2016-01-11 01:29:55Z reddawg $

*****************************************************************************************/

extern "C" 
{	
#include <lib/kmalloc.h>
#include <sys/video.h>
void __pure_virtual() { while(1); }

void __cxa_pure_virtual() { while(1); }

/* Don't plan on exiting the kernel...so do nothing. */
int __cxa_atexit(void (*func)(void *), void * arg, void * d) { return 0; }

void __dso_handle() { while(1); }
}

#include <lib/libcpp.h>

void * operator new[](unsigned size)
{
    return kmalloc(size);
}

void operator delete[](void * ptr)
{
    kfree(ptr);

    return;
}

void * operator new(unsigned size)
{
    void * ptr = kmalloc(size);
    return ptr;
}

void operator delete(void * ptr)
{
    kfree(ptr);
    return;
}

/***
 $Log: libcpp.cc,v $
 Revision 1.1.1.1  2006/06/01 12:46:16  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:12  reddawg
 no message

 Revision 1.5  2004/09/08 22:04:10  apwillia
 Added calling of static constructors, commented out tty_printf in kprintf (due to deadlock)

 Revision 1.4  2004/07/20 22:58:33  reddawg
 retiring to the laptop for the night must sync in work to resume from there

 Revision 1.3  2004/07/02 12:28:24  reddawg
 Changes for new libc, someone please test that the kernel still works

 Revision 1.2  2004/05/19 04:07:43  reddawg
 kmalloc(size,pid) no more it is no kmalloc(size); the way it should of been

 Revision 1.1.1.1  2004/04/15 12:07:10  reddawg
 UbixOS v1.0

 Revision 1.2  2004/04/13 16:36:33  reddawg
 Changed our copyright, it is all now under a BSD-Style license

 END
 ***/
