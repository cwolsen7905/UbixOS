/*****************************************************************************************
 Copyright (c) 2002 The UbixOS Project
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

#include <assert.h>
#include <ubixfs/dirCache.h>
#include <ubixfs/ubixfs.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <lib/string.h>

#include <ubixos/spinlock.h>

static spinLock_t dca_spinLock = SPIN_LOCK_INITIALIZER;

static
struct directoryEntry *
ubixfs_findName(struct directoryEntry * dirList, uInt32 size, char * name) {
  unsigned int i;

  if (dirList == NULL || name == NULL) return NULL;
  //UBU kprintf("dirList: [0x%X],name: [0x%X]\n",dirList,name);
  for (i = 0; i < (size / sizeof(struct directoryEntry)) ; i++) {
    if (strcmp(dirList[i].fileName, name) == 0) return &dirList[i];
  } /* for */
  return NULL;
} /* ubixfs_findName */

struct cacheNode *
ubixfs_cacheFind(struct cacheNode * head, char * name) {
  struct cacheNode * tmp = head;
  struct directoryEntry * dirList = NULL;
  unsigned int i = 0x0;
  char dirName[256];
  char * nextDir = NULL;
/* kprintf("looking for %s\n", name);  */
assert(name);
assert(head);
assert(*name);
  spinLock(&dca_spinLock);
  spinUnlock(&dca_spinLock);
  if (name == NULL || head == NULL) return NULL;
  if (*name == '\0') return NULL;

  /* 
   * walk down the tree recursively until we find the node we're looking
   * for
   */
  i = 0;
  while (name[i] != '\0' && name[i] != '/' && i < sizeof(dirName)) {
    dirName[i] = name[i];
    i++;
  } /* while */
  assert(i < sizeof(dirName));
  if (i == sizeof(dirName)) return NULL;

  if (i == 0) dirName[i++] = '/';

  dirName[i] = '\0';
  
  nextDir = &name[i];
  if (*nextDir == '/') nextDir++;

  /* 
   * nextDir points to the next dir
   * name points to a null terminated directory name
   * if nextDir isn't null, then make sure that this dir is present
   */
/* kprintf("nextdir: %s  -- dirName: %s\n", nextDir, dirName); */
  if (*nextDir != '\0') {
    while (tmp != NULL) {
      //UBU kprintf("tmp->name: [0x%X],dirName: [0x%X]\n",tmp->name,dirName);
      if (strcmp(tmp->name, dirName) == 0) {

        if ((*tmp->attributes & typeFile) == typeFile 
            || tmp->fileListHead == NULL) {

          /* if we're here, then there are no subdirs cached to look through */
          dirList = ubixfs_findName((struct directoryEntry *)head->info,
                                    *head->size, nextDir);
          if (dirList == NULL) return NULL;
/* kprintf("creating new node %s", dirList->fileName); */
          tmp = ubixfs_cacheAdd(tmp, ubixfs_cacheNew(dirList->fileName));
          tmp->attributes = &dirList->attributes;
          tmp->permissions = &dirList->permissions;
          tmp->size = &dirList->size;
/* kprintf("   size: %d\n", *tmp->size); */
          tmp->startCluster = &dirList->startCluster;
          tmp->present = 0;
          return tmp;
        } else {
          return ubixfs_cacheFind(tmp->fileListHead, nextDir);
        }
      } /* if */
      tmp = tmp->next;
    } /* while */
    /* it wasn't present, return NULL */
    return NULL;
  } /* if */ 

  /*
   * if nextDir was null, then we're at the bottom level. Look for the
   * dir listing here 
   */
  while (tmp != NULL) {

    assert(tmp->name);
    assert(name);
/* don't forget to check to see if it's a directory */
    //UBU kprintf("tmpName: [0x%X], name: [0x%X]\n",tmp->name,name);
    if (strcmp(tmp->name, name) == 0) {

      /* 
       * we found the node. Move it to the front of the list 
       * (if it isn't already)
       */
#if 0
      assert(tmp->parent);
      if (tmp != tmp->parent->fileListHead) {

        /* if we're the tail, point the tail to our prev */
        if (tmp == tmp->parent->fileListTail) {
          tmp->parent->fileListTail = tmp->prev;
        } /* if */

        if (tmp->next != NULL) tmp->next->prev = tmp->prev;
        if (tmp->prev != NULL) tmp->prev->next = tmp->next;
        tmp->next = tmp->parent->fileListHead;
        tmp->prev = NULL;
        tmp->parent->fileListHead = tmp;
      } /* if */
#endif
      return tmp;
    } /* if */
    tmp = tmp->next;
  } /* while */
    /* if we're here, then one level of the dir isn't cached */

  tmp = head->parent;
  assert(tmp);
  assert(tmp->info);
  dirList = ubixfs_findName((struct directoryEntry *)tmp->info, 
                            *tmp->size, name);
  if (dirList == NULL) return NULL;
/* kprintf("creating new node/size %s/%d", dirList->fileName, dirList->size);*/
  tmp = ubixfs_cacheAdd(tmp, ubixfs_cacheNew(dirList->fileName));
  tmp->attributes = &dirList->attributes;
  tmp->permissions = &dirList->permissions;
  tmp->size = &dirList->size;
/* kprintf("   size: %d\n", *tmp->size); */
  tmp->startCluster = &dirList->startCluster;
  tmp->present = 0;
  return tmp;
#if 0
  return NULL;  /* couldn't find it */
#endif
} /* ubixfs_cacheFind */

struct cacheNode * 
ubixfs_cacheNew(const char * name) {
  struct cacheNode * tmp = kmalloc(sizeof(struct cacheNode));
  assert(tmp);
  tmp->parent = tmp;
  tmp->prev = tmp->next = tmp->fileListHead = tmp->fileListTail = NULL;
  tmp->info = NULL;
  tmp->size = NULL;
  tmp->present = tmp->dirty = 0;
  tmp->startCluster = NULL;
  tmp->attributes = NULL;
  tmp->permissions = NULL;
  tmp->name = (char *)kmalloc(strlen(name)+1);
  strcpy(tmp->name, name);
  return tmp;
} /* ubixfs_cacheNew */

void
ubixfs_cacheDelete(struct cacheNode ** head) {
  struct cacheNode * tmp = NULL;
  struct cacheNode * del = NULL;
  
  if (head == NULL) return;
  if (*head == NULL) return;

  tmp = *head;
  while (tmp != NULL) {
    /* if there are any child nodes, delete them first */

    /* 
     * the following commented out ``if'' statement is redundant, since it 
     * will be caught with the above checks
     */
    /* if (tmp->fileListHead != NULL) */
    ubixfs_cacheDelete(&tmp->fileListHead);

    kfree(tmp->info);
    kfree(tmp->name);
    del = tmp;
    tmp = tmp->next;
    kfree(del);
  } /* while */
  *head = NULL;
  return;
} /* deleteNode */
#if 0
void
addNode(struct cacheNode ** node, struct cacheNode * newNode) {
  if (node == NULL) return;
  newNode->next = *node;
  if (*node != NULL) (*node)->prev = newNode;
  newNode->prev = NULL;
  *node = newNode;
  return;
} /* addNode */
#endif

struct cacheNode *
ubixfs_cacheAdd(struct cacheNode *node, struct cacheNode * newNode) {
  struct cacheNode * tmp;
 
  assert(node);
  spinLock(&dca_spinLock);
  newNode->parent = node;
  newNode->next = node->fileListHead;
  newNode->prev = NULL;
  if (node->fileListHead == NULL) 
    node->fileListTail = newNode;
  else
    node->fileListHead->prev = newNode;

  node->fileListHead = newNode;
  tmp = node->fileListHead;
  spinUnlock(&dca_spinLock);
  return tmp;
} /* ubixfs_cacheAdd */

/***
 $Log$
 Revision 1.1.1.1  2006/06/01 12:46:17  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:40  reddawg
 no message

 Revision 1.30  2004/08/14 11:23:02  reddawg
 Changes

 Revision 1.29  2004/08/09 12:58:05  reddawg
 let me know when you got the surce

 Revision 1.28  2004/08/01 17:58:39  flameshadow
 chg: fixed string allocation bug in ubixfs_cacheNew()

 Revision 1.27  2004/07/28 17:24:13  flameshadow
 chg: no comment

 Revision 1.26  2004/07/28 17:07:29  flameshadow
 chg: re-added moving cached nodes to the front of the list when found
 add: added an assert() in ubixfs.c

 Revision 1.25  2004/07/27 19:24:31  flameshadow
 chg: reduced the number of debugging statements in the kernel.

 Revision 1.24  2004/07/27 12:02:01  reddawg
 chg: fixed marks bug readFile did a lookup which is why it looked like it was loopping so much

 Revision 1.23  2004/07/27 09:05:43  flameshadow
 chg: fixed file not found bug. Still can't find looping issue

 Revision 1.22  2004/07/27 04:05:20  flameshadow
 chg: kinda fixed it. Added bunches of debug info

 Revision 1.21  2004/07/25 22:21:52  flameshadow
 chg: re-enabled kprintf() in ubixfs_cacheFind()

 Revision 1.20  2004/07/24 17:19:24  flameshadow
 chg: Temporarily disabled the moving of the found cache node to the front
      of the list. It seems to cause problems later (race condition, possibly)

 Revision 1.19  2004/07/22 23:01:51  reddawg
 Ok checking in before I sleep

 Revision 1.18  2004/07/22 19:54:50  flameshadow
 chg: works now. Thanx ubu

 Revision 1.17  2004/07/22 19:01:59  flameshadow
 chg: more directory and file caching

 Revision 1.16  2004/07/22 16:34:32  flameshadow
 add: file and dir caching kinda work

 Revision 1.15  2004/07/21 22:43:18  flameshadow
 one more try

 Revision 1.14  2004/07/21 22:42:25  flameshadow
 try it now

 Revision 1.13  2004/07/21 22:40:27  flameshadow
 weird

 Revision 1.12  2004/07/21 22:18:37  flameshadow
 chg: renamed subDirsHead/Tail members of cacheNode to fileListHead/Tail

 Revision 1.11  2004/07/21 22:12:22  flameshadow
 add: attributes tag in cacheNode
 add: setting of attributes in ubixfx_cacheNew() and ubixfs_cacheFind()

 Revision 1.10  2004/07/21 22:07:18  flameshadow
 chg: renamed caching functions (again)

 Revision 1.9  2004/07/21 22:00:04  flameshadow
 chg: ubixfws_dirCacheAdd now returns a pointer to the node it added
 chg: minor fix in ubixfs_dirCacheFind()

 Revision 1.6  2004/07/21 14:43:14  flameshadow
 add: added cwc (current working container) to the osInfo strut

 Revision 1.5  2004/07/20 19:21:30  reddawg
 You like leaving out $Log$
 You like leaving out Revision 1.1.1.1  2006/06/01 12:46:17  reddawg
 You like leaving out ubix2
 You like leaving out
 You like leaving out Revision 1.2  2005/10/12 00:13:37  reddawg
 You like leaving out Removed
 You like leaving out
 You like leaving out Revision 1.1.1.1  2005/09/26 17:24:40  reddawg
 You like leaving out no message
 You like leaving out
 You like leaving out Revision 1.30  2004/08/14 11:23:02  reddawg
 You like leaving out Changes
 You like leaving out
 You like leaving out Revision 1.29  2004/08/09 12:58:05  reddawg
 You like leaving out let me know when you got the surce
 You like leaving out
 You like leaving out Revision 1.28  2004/08/01 17:58:39  flameshadow
 You like leaving out chg: fixed string allocation bug in ubixfs_cacheNew()
 You like leaving out
 You like leaving out Revision 1.27  2004/07/28 17:24:13  flameshadow
 You like leaving out chg: no comment
 You like leaving out
 You like leaving out Revision 1.26  2004/07/28 17:07:29  flameshadow
 You like leaving out chg: re-added moving cached nodes to the front of the list when found
 You like leaving out add: added an assert() in ubixfs.c
 You like leaving out
 You like leaving out Revision 1.25  2004/07/27 19:24:31  flameshadow
 You like leaving out chg: reduced the number of debugging statements in the kernel.
 You like leaving out
 You like leaving out Revision 1.24  2004/07/27 12:02:01  reddawg
 You like leaving out chg: fixed marks bug readFile did a lookup which is why it looked like it was loopping so much
 You like leaving out
 You like leaving out Revision 1.23  2004/07/27 09:05:43  flameshadow
 You like leaving out chg: fixed file not found bug. Still can't find looping issue
 You like leaving out
 You like leaving out Revision 1.22  2004/07/27 04:05:20  flameshadow
 You like leaving out chg: kinda fixed it. Added bunches of debug info
 You like leaving out
 You like leaving out Revision 1.21  2004/07/25 22:21:52  flameshadow
 You like leaving out chg: re-enabled kprintf() in ubixfs_cacheFind()
 You like leaving out
 You like leaving out Revision 1.20  2004/07/24 17:19:24  flameshadow
 You like leaving out chg: Temporarily disabled the moving of the found cache node to the front
 You like leaving out      of the list. It seems to cause problems later (race condition, possibly)
 You like leaving out
 You like leaving out Revision 1.19  2004/07/22 23:01:51  reddawg
 You like leaving out Ok checking in before I sleep
 You like leaving out
 You like leaving out Revision 1.18  2004/07/22 19:54:50  flameshadow
 You like leaving out chg: works now. Thanx ubu
 You like leaving out
 You like leaving out Revision 1.17  2004/07/22 19:01:59  flameshadow
 You like leaving out chg: more directory and file caching
 You like leaving out
 You like leaving out Revision 1.16  2004/07/22 16:34:32  flameshadow
 You like leaving out add: file and dir caching kinda work
 You like leaving out
 You like leaving out Revision 1.15  2004/07/21 22:43:18  flameshadow
 You like leaving out one more try
 You like leaving out
 You like leaving out Revision 1.14  2004/07/21 22:42:25  flameshadow
 You like leaving out try it now
 You like leaving out
 You like leaving out Revision 1.13  2004/07/21 22:40:27  flameshadow
 You like leaving out weird
 You like leaving out
 You like leaving out Revision 1.12  2004/07/21 22:18:37  flameshadow
 You like leaving out chg: renamed subDirsHead/Tail members of cacheNode to fileListHead/Tail
 You like leaving out
 You like leaving out Revision 1.11  2004/07/21 22:12:22  flameshadow
 You like leaving out add: attributes tag in cacheNode
 You like leaving out add: setting of attributes in ubixfx_cacheNew() and ubixfs_cacheFind()
 You like leaving out
 You like leaving out Revision 1.10  2004/07/21 22:07:18  flameshadow
 You like leaving out chg: renamed caching functions (again)
 You like leaving out
 You like leaving out Revision 1.9  2004/07/21 22:00:04  flameshadow
 You like leaving out chg: ubixfws_dirCacheAdd now returns a pointer to the node it added
 You like leaving out chg: minor fix in ubixfs_dirCacheFind()
 You like leaving out
 You like leaving out Revision 1.6  2004/07/21 14:43:14  flameshadow
 You like leaving out add: added cwc (current working container) to the osInfo strut
 You like leaving out so i don't know what is going on eh?

 END
 ***/
 

