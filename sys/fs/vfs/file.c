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

#include <ubixos/sched.h>
#include <vfs/vfs.h>
#include <ubixos/vitals.h>
#include <ubixos/kpanic.h>
#include <ubixos/spinlock.h>
#include <lib/kmalloc.h>
#include <string.h>
#include <vmm/paging.h>
#include <lib/kprintf.h>
#include <assert.h>

static struct spinLock fdTable_lock = SPIN_LOCK_INITIALIZER;

fileDescriptor *fdTable = 0x0;

fileDescriptor *vfs_fileTable = 0x0;

int sys_fwrite( struct thread *td, struct sys_fwrite_args *uap ) {
  char *t = uap->buf;

  if ( uap->fd == 0x0 )
    tty_print( (char *) uap->buf, _current->term );
  else {
    kprintf( "uap->size: %i, FD: [0x%X], BUF: [0x%X][%c]\n", uap->nbytes, uap->fd, uap->buf, t[0] );
    fwrite( uap->buf, uap->nbytes, 1, uap->fd->fd );
  }

  td->td_retval[0] = 0x0;

  return (0);
}

/* USER */

void sysFwrite( char *ptr, int size, userFileDescriptor *userFd ) {
  if ( userFd == 0x0 ) {
    tty_print( ptr, _current->term );
  }
  else {
    fwrite( ptr, size, 1, userFd->fd );
  }
  return;
}

int sys_fgetc( struct thread *td, struct sys_fgetc_args *args ) {
  char c;

  if ( args->FILE->fd == 0x0 ) {

    while ( 1 ) {

      if ( _current->term == tty_foreground ) {
        c = getchar();

        if ( c != 0x0 ) {
          td->td_retval[0] = c;
          return (0);
        }

        sched_yield();

      }
      else {
        sched_yield();
      }
      /*
       else {
       kprintf("Waking Task: %i\n",tty_foreground->owner);
       sched_setStatus(tty_foreground->owner,READY);
       kprintf("Sleeping Task: %i\n",_current->id);
       sched_setStatus(_current->id,WAIT);
       sched_yield();
       }
       */
    }
  }
  else {
    c = fgetc( args->FILE->fd );
    td->td_retval[0] = c;
    return (0);
  }
}

void sysRmDir() {
  return;
}

//int sys_fseek(userFileDescriptor *userFd,long offset,int whence) {
int sys_fseek( struct thread *td, struct sys_fseek_args *args ) {
  // TODO : coredump?
  if ( args->FILE == NULL ) {
    td->td_retval[0] = -1;
    return (-1);
  }

  if ( args->FILE->fd == NULL ) {
    td->td_retval[0] = -1;
    return (-1);
  }

  args->FILE->fd->offset = args->offset + args->whence;
  return (0);
}

int sys_chdir( struct thread *td, struct sys_chdir_args *args ) {
  if ( strstr( args->path, ":" ) == 0x0 ) {
    sprintf( _current->oInfo.cwd, "%s%s", _current->oInfo.cwd, args->path );
  }
  else {
    sprintf( _current->oInfo.cwd, args->path );
  }
  return (0);
}

int sysUnlink( const char *path, int *retVal ) {
  *retVal = unlink( path );
  return (*retVal);
}

/************************************************************************

 Function: void sysFopen();
 Description: Opens A File Descriptor For A User Task
 Notes:

 ************************************************************************/
//void sysFopen(const char *file,char *flags,userFileDescriptor *userFd) {
int sys_fopen( struct thread *td, struct sys_fopen_args *args ) {
  if ( args->FILE == NULL ) {
    kprintf( "Error: userFd == NULL, File: %s, Line: %i\n", __FILE__, __LINE__ );
    return (-1);
  }

  args->FILE->fd = fopen( args->path, args->mode );
  if ( args->FILE->fd != 0x0 ) {
    args->FILE->fdSize = args->FILE->fd->size;
  }
  /* Return */
  return (0);
}

/************************************************************************

 Function: void sysFread();
 Description: Reads SIZE Bytes From The userFd Into DATA
 Notes:

 ************************************************************************/
int sys_fread( struct thread *td, struct sys_fread_args *args ) {
//void *data,long size,userFileDescriptor *userFd) {

  /* TODO : coredump? */
  if ( args->FILE == NULL )
    return (-1);

  if ( args->FILE->fd == NULL )
    return (-1);

  return (fread( args->ptr, args->size, args->nmemb, args->FILE->fd ));
}

/************************************************************************

 Function: void sysFclse();
 Description: Closes A File Descriptor For A User Task
 Notes:

 ************************************************************************/
int sys_fclose( struct thread *td, struct sys_fclose_args *args ) {
  if ( args->FILE == NULL ) {
    return (-1);
  }
  if ( args->FILE == NULL ) {
    return (-1);
  }

  /* Return */
  return (fclose( args->FILE ));
}

/* KERNEL */

size_t fread( void *ptr, size_t size, size_t nmemb, fileDescriptor *fd ) {
  size_t i = 0x0;


  asm("nop");
  asm("nop");
  if ( fd == 0x0 )
    return (0x0);
  asm("nop");
  asm("nop");

  if ( nmemb == 0x0 )
    nmemb = 1; //Temp Fix
  asm("nop");
  asm("nop");

  assert( fd );
  asm("nop");
  asm("nop");
  assert( fd->mp );
  asm("nop");
  asm("nop");
  assert( fd->mp->fs );
  asm("nop");
  asm("nop");

  i = fd->mp->fs->vfsRead( fd, ptr, fd->offset, size * nmemb );

  fd->offset += size * nmemb;

  //kprintf("fread: %i:%i",i,size *nmemb);

  return (i);
}

size_t fwrite( void *ptr, int size, int nmemb, fileDescriptor *fd ) {
  if ( fd != 0x0 ) {
    fd->mp->fs->vfsWrite( fd, ptr, fd->offset, size * nmemb );
    fd->offset += size * nmemb;
  }
  return (0x0);
}

int fseek( fileDescriptor *tmpFd, long offset, int whence ) {
  tmpFd->offset = offset + whence;
  return (tmpFd->offset);
}

/************************************************************************

 Function: int feof(fileDescriptor *fd)
 Description: Check A File Descriptor For EOF And Return Result
 Notes:

 ************************************************************************/
int feof( fileDescriptor *fd ) {
  if ( fd->status == fdEof ) {
    return (-1);
  }
  return (0);
}

/************************************************************************

 Function: int fputc(int ch,fileDescriptor *fd)
 Description: This Will Write Character To FD
 Notes:

 ************************************************************************/
int fputc( int ch, fileDescriptor *fd ) {
  if ( fd != 0x0 ) {
    ch = fd->mp->fs->vfsWrite( fd, (char *) ch, fd->offset, 1 );
    fd->offset++;
    return (ch);
  }
  /* Return NULL If FD Is Not Found */
  return (0x0);
}

/************************************************************************

 Function: int fgetc(fileDescriptor *fd)
 Description: This Will Return The Next Character In A FD Stream
 Notes:

 ************************************************************************/
int fgetc( fileDescriptor *fd ) {
  int ch = 0x0;
  /* If Found Return Next Char */
  if ( fd != 0x0 ) {
    fd->mp->fs->vfsRead( fd, (char *) &ch, fd->offset, 1 );
    fd->offset++;
    return (ch);
  }

  /* Return NULL If FD Is Not Found */
  return (0x0);
}

/************************************************************************

 Function: fileDescriptor *fopen(const char *file,cont char *flags)
 Description: This Will Open A File And Return A File Descriptor
 Notes:

 08/05/02 - Just Started A Rewrite Of This Function Should Work Out Well

 ************************************************************************/

fileDescriptor *fopen( const char *file, const char *flags ) {
  int i = 0x0;
  char *path = 0x0;
  char *mountPoint = 0x0;
  char fileName[1024];
  fileDescriptor *tmpFd = 0x0;

  /* Allocate Memory For File Descriptor */
  if ( (tmpFd = (fileDescriptor *) kmalloc( sizeof(fileDescriptor) )) == 0x0 ) {
    kprintf( "Error: tmpFd == NULL, File: %s, Line: %i\n", __FILE__, __LINE__ );
    return (NULL);
  }

  strcpy( fileName, file );

  if ( strstr( fileName, ":" ) ) {
    mountPoint = (char *) strtok( (char *) &fileName, ":" );
    path = strtok( NULL, "\n" );
  }
  else {
    path = fileName;
  }

  if ( path[0] == '/' )
    strcpy( tmpFd->fileName, path );
  else
    sprintf( tmpFd->fileName, "/%s", path );

  /* Find our mount point or set default to sys */
  if ( mountPoint == 0x0 )
    tmpFd->mp = vfs_findMount( "sys" );
  else
    tmpFd->mp = vfs_findMount( mountPoint );

  if ( tmpFd->mp == 0x0 ) {
    kprintf( "Mount Point Bad\n" );
    return (0x0);
  }

  /* This Will Set Up The Descriptor Modes */
  tmpFd->mode = 0;
  for ( i = 0; '\0' != flags[i]; i++ ) {
    switch ( flags[i] ) {
      case 'w':
        case 'W':
        tmpFd->mode |= fileWrite;
        break;
      case 'r':
        case 'R':
        tmpFd->mode |= fileRead;
        break;
      case 'b':
        case 'B':
        tmpFd->mode |= fileBinary;
        break;
      case 'a':
        case 'A':
        tmpFd->mode |= fileAppend;
        break;
      default:
        kprintf( "Invalid mode '%c' for fopen\n", flags[i] );
        break;
    }
  }
  /* Search For The File */
  if ( tmpFd->mp->fs->vfsOpenFile( tmpFd->fileName, tmpFd ) == 0x1 ) {
    /* If The File Is Found Then Set Up The Descriptor */

    /* in order to save resources we will allocate the buffer later when it is needed */

    tmpFd->buffer = (char *) kmalloc( 4096 );
    if ( tmpFd->buffer == 0x0 )
        {
      kfree( tmpFd );
      kprintf( "Error: tmpFd->buffer == NULL, File: %s, Line: %i\n", __FILE__, __LINE__ );
      spinUnlock( &fdTable_lock );
      return 0x1;
    }
    /* Set Its Status To Open */
    tmpFd->status = fdOpen;

    /* Initial File Offset Is Zero */
    tmpFd->offset = 0;
    tmpFd->prev = 0x0;

    /* we do not want to be in a spinlock longer than we need to, so
     it has been moved to here. */
    spinLock( &fdTable_lock );

    /* Increment Number Of Open Files */
    systemVitals->openFiles++;

    tmpFd->next = fdTable;

    if ( fdTable != 0x0 )
      fdTable->prev = tmpFd;

    fdTable = tmpFd;

    spinUnlock( &fdTable_lock );

    /* Return The FD */
    return (tmpFd);
  }
  else {
    kfree( tmpFd->buffer );
    kfree( tmpFd );
    spinUnlock( &fdTable_lock );
    //MrOlsen (2016-01-13) NOTE: We don't need this right now kprintf("File Not Found? %s\n",file);
    return (NULL);
  }

  /* Return NULL */
  return (0x0);
}

/************************************************************************

 Function: int fclose(fileDescriptor *fd);
 Description: This Will Close And Free A File Descriptor
 Notes:

 ************************************************************************/
int fclose( fileDescriptor *fd ) {
  fileDescriptor *tmpFd = 0x0;
  assert( fd );

  spinLock( &fdTable_lock );

  for ( tmpFd = fdTable; tmpFd != 0x0; tmpFd = tmpFd->next ) {
    if ( tmpFd == fd ) {
      if ( tmpFd->prev )
        tmpFd->prev->next = tmpFd->next;
      if ( tmpFd->next )
        tmpFd->next->prev = tmpFd->prev;

      if ( tmpFd == fdTable )
        fdTable = tmpFd->next;

      systemVitals->openFiles--;
      spinUnlock( &fdTable_lock );
      if ( tmpFd->buffer != NULL )
        kfree( tmpFd->buffer );
      kfree( tmpFd );
      return (0x0);
    }
  }

  spinUnlock( &fdTable_lock );
  return (0x1);
}

/* UBU */

/************************************************************************

 Function: void sysMkDir(const char *path)
 Description: This Will Create A New Directory
 Notes:

 ************************************************************************/
void sysMkDir( const char *path ) {
  fileDescriptor *tmpFD = 0x0;
  char tmpDir[1024];
  char rootPath[256];
  char *dir = 0x0; //UBU*mountPoint = 0x0;
  char *tmp = 0x0;
  rootPath[0] = '\0';
  dir = (char *) path;

  if ( strstr( path, ":" ) == 0x0 ) {
    sprintf( tmpDir, "%s%s", _current->oInfo.cwd, path );
    dir = (char *) &tmpDir;
  }
  while ( strstr( dir, "/" ) ) {
    if ( rootPath[0] == 0x0 )
      sprintf( rootPath, "%s/", strtok( dir, "/" ) );
    else
      sprintf( rootPath, "%s%s/", rootPath, strtok( dir, "/" ) );
    tmp = strtok( NULL, "\n" );
    dir = tmp;
  }

  //kprintf("rootPath: [%s]\n",rootPath);
  tmpFD = fopen( rootPath, "rb" );

  if ( tmpFD->mp == 0x0 ) {
    kprintf( "Invalid Mount Point\n" );
  }
  tmpFD->mp->fs->vfsMakeDir( dir, tmpFD );

  fclose( tmpFD );

  return;
}

/************************************************************************

 Function: int unlink(const char *node)
 Description: This will unlink a file
 Notes:

 ************************************************************************/

int unlink( const char *node ) {
  char *path = 0x0, *mountPoint = 0x0;
  struct vfs_mountPoint *mp = 0x0;

  path = (char *) strtok( (char *) node, "@" );
  mountPoint = strtok( NULL, "\n" );
  if ( mountPoint == 0x0 ) {
    mp = vfs_findMount( "sys" ); /* _current->oInfo.container; */
  }
  else {
    mp = vfs_findMount( mountPoint );
  }
  if ( mp == 0x0 ) {
    //kpanic("Mount Point Bad");
    return (0x0);
  }
  mp->fs->vfsUnlink( path, mp );
  return (0x0);
}

/***
 END
 ***/

