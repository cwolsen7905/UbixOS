/*****************************************************************************************
 Copyright (c) 2002-2004, 2016 The UbixOS Project
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

 $Id: kern_sysctl.c 168 2016-01-20 00:41:47Z reddawg $

 *****************************************************************************************/

#include <sys/kern_sysctl.h>
#include <sys/sysproto.h>
#include <sys/thread.h>
#include <ubixos/endtask.h>
#include <ubixos/kpanic.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <assert.h>
#include <string.h>

static struct sysctl_entry *ctls = 0x0;

static struct sysctl_entry *sysctl_find( int *, int );

/* This is a cheat for now */
static void def_ctls() {
  int name[CTL_MAXNAME], name_len;
  uInt32 page_val = 0x1000;
  name[0] = 6;
  name[1] = 7;
  name_len = 2;
  sysctl_add( name, name_len, "page_size", &page_val, sizeof(uInt32) );

  /* Clock Rate */
  name[0] = 1;
  name[1] = 12;
  page_val = 0x3E8;
  sysctl_add( name, name_len, "page_size", &page_val, sizeof(uInt32) );

  /* KERN: OS Release */
  name[0] = 1;
  name[1] = 24;
  page_val = 101000;
  sysctl_add( name, name_len, "kern.osreldate", &page_val, sizeof(uInt32) );

  /* KERN: User Stack */
  name[0] = 1;
  name[1] = 33;
  page_val = 0xCBE8000;
  sysctl_add( name, name_len, "page_size", &page_val, sizeof(uInt32) );

  /* KERN: ARND */
  name[0] = 1;
  name[1] = 37;
  page_val = 0x1;
  sysctl_add( name, name_len, "kern_arnd", &page_val, sizeof(uint32_t) );

  /* HW: NCPU */
  name[0] = 6;
  name[1] = 3;
  page_val = 0x1;
  sysctl_add( name, name_len, "hw.ncpu", &page_val, sizeof(uint32_t) );
}

int sysctl_init() {
  struct sysctl_entry *tmpCtl = 0x0;
  if ( ctls != 0x0 ) {
    kprintf( "sysctl already Initialized\n" );
    while ( 1 )
      ;
  }

  ctls = (struct sysctl_entry *) kmalloc( sizeof(struct sysctl_entry) );
  ctls->prev = 0x0;
  ctls->id = CTL_UNSPEC;
  ctls->children = 0x0;
  sprintf( ctls->name, "unspec" );

  tmpCtl = (struct sysctl_entry *) kmalloc( sizeof(struct sysctl_entry) );
  tmpCtl->prev = ctls;
  tmpCtl->id = CTL_KERN;
  tmpCtl->children = 0x0;
  sprintf( tmpCtl->name, "kern" );
  ctls->next = tmpCtl;

  tmpCtl->next = (struct sysctl_entry *) kmalloc( sizeof(struct sysctl_entry) );
  tmpCtl->next->prev = tmpCtl;
  tmpCtl = tmpCtl->next;
  tmpCtl->id = CTL_VM;
  tmpCtl->children = 0x0;
  sprintf( tmpCtl->name, "vm" );

  tmpCtl->next = (struct sysctl_entry *) kmalloc( sizeof(struct sysctl_entry) );
  tmpCtl->next->prev = tmpCtl;
  tmpCtl = tmpCtl->next;
  tmpCtl->id = CTL_VFS;
  tmpCtl->children = 0x0;
  sprintf( tmpCtl->name, "vfs" );

  tmpCtl->next = (struct sysctl_entry *) kmalloc( sizeof(struct sysctl_entry) );
  tmpCtl->next->prev = tmpCtl;
  tmpCtl = tmpCtl->next;
  tmpCtl->id = CTL_NET;
  tmpCtl->children = 0x0;
  sprintf( tmpCtl->name, "net" );

  tmpCtl->next = (struct sysctl_entry *) kmalloc( sizeof(struct sysctl_entry) );
  tmpCtl->next->prev = tmpCtl;
  tmpCtl = tmpCtl->next;
  tmpCtl->id = CTL_DEBUG;
  tmpCtl->children = 0x0;
  sprintf( tmpCtl->name, "debug" );

  tmpCtl->next = (struct sysctl_entry *) kmalloc( sizeof(struct sysctl_entry) );
  tmpCtl->next->prev = tmpCtl;
  tmpCtl = tmpCtl->next;
  tmpCtl->id = CTL_HW;
  tmpCtl->children = 0x0;
  sprintf( tmpCtl->name, "hw" );

  tmpCtl->next = (struct sysctl_entry *) kmalloc( sizeof(struct sysctl_entry) );
  tmpCtl->next->prev = tmpCtl;
  tmpCtl = tmpCtl->next;
  tmpCtl->id = CTL_MACHDEP;
  tmpCtl->children = 0x0;
  sprintf( tmpCtl->name, "machdep" );

  tmpCtl->next = (struct sysctl_entry *) kmalloc( sizeof(struct sysctl_entry) );
  tmpCtl->next->prev = tmpCtl;
  tmpCtl = tmpCtl->next;
  tmpCtl->id = CTL_USER;
  tmpCtl->children = 0x0;
  sprintf( tmpCtl->name, "user" );

  tmpCtl->next = (struct sysctl_entry *) kmalloc( sizeof(struct sysctl_entry) );
  tmpCtl->next->prev = tmpCtl;
  tmpCtl = tmpCtl->next;
  tmpCtl->id = CTL_P1003_1B;
  tmpCtl->children = 0x0;
  sprintf( tmpCtl->name, "p1003_1b" );

  tmpCtl->next = (struct sysctl_entry *) kmalloc( sizeof(struct sysctl_entry) );
  tmpCtl->next->prev = tmpCtl;
  tmpCtl = tmpCtl->next;
  tmpCtl->id = CTL_UBIX;
  tmpCtl->children = 0x0;
  sprintf( tmpCtl->name, "ubix" );

  def_ctls();

  return (0x0);
}

int __sysctl( struct thread *td, struct sysctl_args *uap ) {
  struct sysctl_entry *tmpCtl = 0x0;
  int i = 0;

  if ( ctls == 0x0 )
    K_PANIC( "sysctl not initialized" );

  if ( uap->newlen < 0 ) {
    kprintf( "Changing Not supported yet.\n" );
    endTask( _current->id );
  }

  tmpCtl = sysctl_find( uap->name, uap->namelen );
  if ( tmpCtl == 0x0 ) {
    kprintf( "Invalid CTL\n" );
    for ( i = 0x0; i < uap->namelen; i++ )
      kprintf( "(%i)", uap->name[i] );
    kprintf( "\n" );
    endTask( _current->id );
  }

  if ( (uint32_t) uap->oldlenp < tmpCtl->val_len )
    memcpy( uap->old, tmpCtl->value, (uInt32) uap->oldlenp );
  else
    memcpy( uap->old, tmpCtl->value, tmpCtl->val_len );

  td->td_retval[0] = 0x0;

  return (0x0);
}

int sys_sysctl( struct thread *td, struct sys_sysctl_args *args ) {
  struct sysctl_entry *tmpCtl = 0x0;
  int i = 0;

  if ( ctls == 0x0 )
    K_PANIC( "sysctl not initialized" );

  if ( args->newlenp < 0 ) {
    kprintf( "Changing Not supported yet.\n" );
    endTask( _current->id );
  }

  tmpCtl = sysctl_find( args->name, args->namelen );

  if ( tmpCtl == 0x0 ) {
    kprintf( "Invalid CTL\n" );
    for ( i = 0x0; i < args->namelen; i++ )
      kprintf( "(%i)", (int)args->name[i] );
    kprintf( "\n" );
    td->td_retval[0] = -1;
    return (-1);
  }

  if ( (uint32_t) args->oldlenp < tmpCtl->val_len )
    memcpy( args->oldp, tmpCtl->value, (uInt32) args->oldlenp );
  else
    memcpy( args->oldp, tmpCtl->value, tmpCtl->val_len );

  td->td_retval[0] = 0x0;

  return (0x0);
}

static struct sysctl_entry *sysctl_find( int *name, int namelen ) {
  int i = 0x0;
  struct sysctl_entry *tmpCtl = 0x0;
  struct sysctl_entry *lCtl = ctls;

  /* Loop Name Len */
  for ( i = 0x0; i < namelen; i++ ) {
    for ( tmpCtl = lCtl; tmpCtl != 0x0; tmpCtl = tmpCtl->next ) {
      //kprintf("ctlName: [%s], ctlId; [%i]\n",tmpCtl->name,tmpCtl->id);
      if ( tmpCtl->id == name[i] ) {
        if ( (i + 1) == namelen ) {
          return (tmpCtl);
        }
        lCtl = tmpCtl->children;
        break;
      }
    }
  }
  return (0x0);
}

int sysctl_add( int *name, int namelen, char *str_name, void *buf, int buf_size ) {
  struct sysctl_entry *tmpCtl = 0x0;
  struct sysctl_entry *newCtl = 0x0;

  /* Check if it exists */
  tmpCtl = sysctl_find( name, namelen );
  if ( tmpCtl != 0x0 ) {
    kprintf( "Node Exists!\n" );
    while ( 1 )
      ;
  }

  /* Get Parent Node */
  tmpCtl = sysctl_find( name, namelen - 1 );
  if ( tmpCtl == 0x0 ) {
    kprintf( "Parent Node Non Existant\n" );
    return (-1);
  }
  if ( tmpCtl->children == 0x0 ) {
    tmpCtl->children = (struct sysctl_entry *) kmalloc( sizeof(struct sysctl_entry) );
    tmpCtl->children->children = 0x0;
    tmpCtl->children->prev = 0x0;
    tmpCtl->children->next = 0x0;
    tmpCtl->children->id = name[namelen - 1];
    sprintf( tmpCtl->children->name, str_name );
    tmpCtl->children->value = (void *) kmalloc( buf_size );
    memcpy( tmpCtl->children->value, buf, buf_size );
    tmpCtl->children->val_len = buf_size;
  }
  else {
    newCtl = (struct sysctl_entry *) kmalloc( sizeof(struct sysctl_entry) );
    newCtl->prev = 0x0;
    newCtl->next = tmpCtl->children;
    newCtl->children = 0x0;
    newCtl->id = name[namelen - 1];
    sprintf( newCtl->name, str_name );
    newCtl->value = (void *) kmalloc( buf_size );
    memcpy( newCtl->value, buf, buf_size );
    newCtl->val_len = buf_size;
    tmpCtl->children->prev = newCtl;
    tmpCtl->children = newCtl;
  }

  return (0x0);
}

/***
 END
 ***/
