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

#include <sys/kern_sysctl.h>
#include <sys/sysproto_posix.h>
#include <sys/thread.h>
#include <ubixos/endtask.h>
#include <ubixos/kpanic.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <assert.h>
#include <string.h>
#include <ubixos/errno.h>
#include <lib/kern_trie.h>

static struct sysctl_entry *ctls = 0x0;

static struct sysctl_entry *sysctl_find(int *, int);
static struct sysctl_entry *sysctl_findMib(char *name, int namelen);

struct Trie *sysctl_headTrie = 0x0;

/* This is a cheat for now */
static void def_ctls() {
  int name[CTL_MAXNAME], name_len;
  uint32_t page_val = 0x1000;
  int32_t usPage_val = 0x0;
  name[0] = 6;
  name[1] = 7;
  name_len = 2;
  sysctl_add(name, name_len, "pagesizes", &page_val, sizeof(uint32_t));

  /* Clock Rate */
  name[0] = 1;
  name[1] = 12;
  page_val = 0x3E8;
  sysctl_add(name, name_len, "page_size", &page_val, sizeof(uInt32));

  /* KERN: OS Release */
  name[0] = 1;
  name[1] = 24;
  page_val = 101000;
  sysctl_add(name, name_len, "kern.osreldate", &page_val, sizeof(uInt32));

  /* KERN: User Stack */
  name[0] = 1;
  name[1] = 33;
  page_val = 0xCBE8000;
  sysctl_add(name, name_len, "page_size", &page_val, sizeof(uInt32));

  /* KERN: ARND */
  name[0] = 1;
  name[1] = 37;
  page_val = 0x1;
  sysctl_add(name, name_len, "kern_arnd", &page_val, sizeof(uint32_t));

  /* HW: NCPU */
  name[0] = 6;
  name[1] = 3;
  page_val = 0x1;
  sysctl_add(name, name_len, "hw.ncpu", &page_val, sizeof(uint32_t));

  name[0] = 9;
  name[1] = 20;
  page_val = 0x4000;
  sysctl_add(name, name_len, "p1003_1b.pagesize", &page_val, sizeof(uint32_t));

  /* XXX 1, 1 */
  name[0] = 1;
  name[1] = 1;
  char s11[4] = "UBIX";
  sysctl_add(name, name_len, "kern.ostype", &s11, 4);

  /* XXX 1, 10 */
  name[0] = 1;
  name[1] = 10;
  char s110[16] = "devel.ubixos.com";
  sysctl_add(name, name_len, "kern.hostname", &s110, 16);

  /* XXX 1, 2 */
  name[0] = 1;
  name[1] = 2;
  char s12[11] = "1.0-RELEASE";
  sysctl_add(name, name_len, "kern.hostname", &s12, 11);

  /* XXX 1, 4 */
  name[0] = 1;
  name[1] = 4;
  char s14[18] = "UbixOS 1.0-RELEASE";
  sysctl_add(name, name_len, "kern.hostname", &s14, 18);

  /* XXX 6, 1 */
  name[0] = 6;
  name[1] = 1;
  char s61[4] = "i386";
  sysctl_add(name, name_len, "kern.hostname", &s61, 4);

  /* XXX 6, 2147482988 */
  name[0] = 6;
  name[1] = 2147482988;
  page_val = 4096;
  sysctl_add(name, name_len, "hw.pagesizes", &page_val, sizeof(u_int32_t));

  name[0] = 2;
  name[1] = 12;
  page_val = 0;
  sysctl_add(name, name_len, "vm.overcommit", &page_val, sizeof(u_int32_t));

  name[0] = 1;
  name[1] = 18;
  usPage_val = 1023;
  sysctl_add(name, name_len, "kern.ngroups", &page_val, sizeof(int32_t));

  /* XXX 6, 1 */
  name[0] = 2;
  name[1] = 134516822;
  char s62[2] = "\0";
  sysctl_add(name, name_len, "kern.msgbuf", &s62, 1);

}

int sysctl_init() {

  struct sysctl_entry *tmpCtl = 0x0;

  if (ctls != 0x0) {
    kprintf("sysctl already Initialized\n");
    while (1)
      ;
  }

  /* Initialize Head Trie */
  sysctl_headTrie = (struct Trie *) kmalloc(sizeof(struct Trie));

  ctls = (struct sysctl_entry *) kmalloc(sizeof(struct sysctl_entry));
  ctls->prev = 0x0;
  ctls->id = CTL_UNSPEC;
  ctls->children = 0x0;
  sprintf(ctls->name, "unspec");

  insert_trieNode(&sysctl_headTrie, ctls->name, ctls);

  tmpCtl = (struct sysctl_entry *) kmalloc(sizeof(struct sysctl_entry));
  tmpCtl->prev = ctls;
  tmpCtl->id = CTL_KERN;
  tmpCtl->children = 0x0;
  sprintf(tmpCtl->name, "kern");
  ctls->next = tmpCtl;
  insert_trieNode(&sysctl_headTrie, ctls->name, ctls);

  tmpCtl->next = (struct sysctl_entry *) kmalloc(sizeof(struct sysctl_entry));
  tmpCtl->next->prev = tmpCtl;
  tmpCtl = tmpCtl->next;
  tmpCtl->id = CTL_VM;
  tmpCtl->children = 0x0;
  sprintf(tmpCtl->name, "vm");
  insert_trieNode(&sysctl_headTrie, ctls->name, ctls);

  tmpCtl->next = (struct sysctl_entry *) kmalloc(sizeof(struct sysctl_entry));
  tmpCtl->next->prev = tmpCtl;
  tmpCtl = tmpCtl->next;
  tmpCtl->id = CTL_VFS;
  tmpCtl->children = 0x0;
  sprintf(tmpCtl->name, "vfs");
  insert_trieNode(&sysctl_headTrie, ctls->name, ctls);

  tmpCtl->next = (struct sysctl_entry *) kmalloc(sizeof(struct sysctl_entry));
  tmpCtl->next->prev = tmpCtl;
  tmpCtl = tmpCtl->next;
  tmpCtl->id = CTL_NET;
  tmpCtl->children = 0x0;
  sprintf(tmpCtl->name, "net");
  insert_trieNode(&sysctl_headTrie, ctls->name, ctls);

  tmpCtl->next = (struct sysctl_entry *) kmalloc(sizeof(struct sysctl_entry));
  tmpCtl->next->prev = tmpCtl;
  tmpCtl = tmpCtl->next;
  tmpCtl->id = CTL_DEBUG;
  tmpCtl->children = 0x0;
  sprintf(tmpCtl->name, "debug");
  insert_trieNode(&sysctl_headTrie, ctls->name, ctls);

  tmpCtl->next = (struct sysctl_entry *) kmalloc(sizeof(struct sysctl_entry));
  tmpCtl->next->prev = tmpCtl;
  tmpCtl = tmpCtl->next;
  tmpCtl->id = CTL_HW;
  tmpCtl->children = 0x0;
  sprintf(tmpCtl->name, "hw");
  insert_trieNode(&sysctl_headTrie, ctls->name, ctls);

  tmpCtl->next = (struct sysctl_entry *) kmalloc(sizeof(struct sysctl_entry));
  tmpCtl->next->prev = tmpCtl;
  tmpCtl = tmpCtl->next;
  tmpCtl->id = CTL_MACHDEP;
  tmpCtl->children = 0x0;
  sprintf(tmpCtl->name, "machdep");
  insert_trieNode(&sysctl_headTrie, ctls->name, ctls);

  tmpCtl->next = (struct sysctl_entry *) kmalloc(sizeof(struct sysctl_entry));
  tmpCtl->next->prev = tmpCtl;
  tmpCtl = tmpCtl->next;
  tmpCtl->id = CTL_USER;
  tmpCtl->children = 0x0;
  sprintf(tmpCtl->name, "user");
  insert_trieNode(&sysctl_headTrie, ctls->name, ctls);

  tmpCtl->next = (struct sysctl_entry *) kmalloc(sizeof(struct sysctl_entry));
  tmpCtl->next->prev = tmpCtl;
  tmpCtl = tmpCtl->next;
  tmpCtl->id = CTL_P1003_1B;
  tmpCtl->children = 0x0;
  sprintf(tmpCtl->name, "p1003_1b");

  insert_trieNode(&sysctl_headTrie, ctls->name, ctls);

  tmpCtl->next = (struct sysctl_entry *) kmalloc(sizeof(struct sysctl_entry));
  tmpCtl->next->prev = tmpCtl;
  tmpCtl = tmpCtl->next;
  tmpCtl->id = CTL_UBIX;
  tmpCtl->children = 0x0;
  sprintf(tmpCtl->name, "ubix");

  insert_trieNode(&sysctl_headTrie, ctls->name, ctls);

  def_ctls();

  return (0x0);
}

int __sysctl(struct thread *td, struct sysctl_args *uap) {
  struct sysctl_entry *tmpCtl = 0x0;
  int i = 0;

  if (ctls == 0x0)
    K_PANIC("sysctl not initialized");

  if (uap->newlen < 0) {
    kprintf("Changing Not supported yet.\n");
    endTask(_current->id);
  }

  tmpCtl = sysctl_find(uap->name, uap->namelen);
  if (tmpCtl == 0x0) {
    kprintf("Invalid CTL\n");
    for (i = 0x0; i < uap->namelen; i++)
      kprintf("(%i)", uap->name[i]);
    kprintf("\n");
    endTask(_current->id);
  }

  if ((uint32_t) uap->oldlenp < tmpCtl->val_len)
    memcpy(uap->oldp, tmpCtl->value, (uInt32) uap->oldlenp);
  else
    memcpy(uap->oldp, tmpCtl->value, tmpCtl->val_len);

  td->td_retval[0] = 0x0;

  return (0x0);
}

int sys_sysctl(struct thread *td, struct sys_sysctl_args *args) {
  struct sysctl_entry *tmpCtl = 0x0;
  struct Trie *tmpTrie = 0x0;
  int i = 0;

  if (ctls == 0x0)
    K_PANIC("sysctl not initialized");

  if (args->newlenp < 0) {
    kprintf("Changing Not supported yet.\n");
    endTask(_current->id);
  }

  /* XXX - Handle search by name */
  if (args->namelen == 2 && args->name[0] == 0 && args->name[1] == 3) {

    tmpTrie = search_trieNode(sysctl_headTrie, args->newp);

    if (tmpTrie != 0x0) {
      tmpCtl = (struct sysctl_entry *)tmpTrie->e;

      //kprintf("<FT: %s:%i>\n", tmpCtl->name,tmpCtl->namelen);

      // tmpCtl = sysctl_findMib(args->newp, args->namelen);
      *args->oldlenp = tmpCtl->namelen *4;
      u_int32_t *oldp = args->oldp;

      for (i=0;i<tmpCtl->namelen;i++)
        oldp[i] = tmpCtl->full_name[i];

      td->td_retval[0] = 0; /* XXX - Very Bad need to store namelen in the struct */

      return(0x0); 
    }
    else {

      #ifdef DEBUG_SYSCTL
      kprintf("%s:%i>name_to_mib: %s\n", __FILE__,__LINE__,args->newp);
      #endif

      td->td_retval[0] = ENOENT;
      return(-1);
    }
  }
  else {
    tmpCtl = sysctl_find(args->name, args->namelen);
  }

  if (tmpCtl == 0x0) {
    kprintf("Invalid CTL(%i): ", args->namelen);
    for (i = 0x0; i < args->namelen; i++)
      kprintf("(%i)", (int) args->name[i]);
    kprintf("\n");
    td->td_retval[0] = -1;
    return (-1);
  }
  /*
  else {
    kprintf("Valid CTL(%i): ", args->namelen);
    for (i = 0x0; i < args->namelen; i++)
      kprintf("(%i)", (int) args->name[i]);
    kprintf("\n");
  }

  kprintf("{%i:%i}\n",args->oldlenp, tmpCtl->val_len);

*/

  if ((uint32_t) args->oldlenp < tmpCtl->val_len)
    memcpy(args->oldp, tmpCtl->value, (uInt32) args->oldlenp);
  else
    memcpy(args->oldp, tmpCtl->value, tmpCtl->val_len);

  td->td_retval[0] = 0x0;

  return (0x0);
}

static struct sysctl_entry *sysctl_find(int *name, int namelen) {
  int i = 0x0;
  struct sysctl_entry *tmpCtl = 0x0;
  struct sysctl_entry *lCtl = ctls;

  /* Loop Name Len */
  for (i = 0x0; i < namelen; i++) {
    for (tmpCtl = lCtl; tmpCtl != 0x0; tmpCtl = tmpCtl->next) {
      //kprintf("ctlName: [%s], ctlId; [%i]\n",tmpCtl->name,tmpCtl->id);
      if (tmpCtl->id == name[i]) {
        if ((i + 1) == namelen) {
          return (tmpCtl);
        }
        lCtl = tmpCtl->children;
        break;
      }
    }
  }
  return (0x0);
}


static struct sysctl_entry *sysctl_findMib(char *name, int namelen) {
  int i = 0x0;
  struct sysctl_entry *tmpCtl = 0x0;
  struct sysctl_entry *lCtl = ctls;

  char *mib = (char *) strtok( (char *) name, "." );


  kprintf("FMIB: %s", mib);

  lCtl = (struct sysctl_entry *) search_trieNode(sysctl_headTrie, mib)->e;
  kprintf("FT: %s", lCtl->name);

  /* Loop Name Len */
  for (i = 0x0; i < namelen; i++) {
  for (tmpCtl = lCtl; tmpCtl != 0x0; tmpCtl = tmpCtl->next) {
    if (strcmp(mib, tmpCtl->name) == 0x0) {
       kprintf("ctlName: [%s], ctlId: [%i]",tmpCtl->name,tmpCtl->id);
        if ((i + 1) == namelen) {
          return (tmpCtl);
        }
        mib = strtok( NULL, "\n" );
        kprintf("SMIB: %s", mib);
        lCtl = tmpCtl->children;
        break;
    }
  }
  }
  return (0x0);
}


int sysctl_add(int *name, int namelen, char *str_name, void *buf, int buf_size) {
  struct sysctl_entry *tmpCtl = 0x0;
  struct sysctl_entry *newCtl = 0x0;
  int i = 0;

  /* Check if it exists */
  tmpCtl = sysctl_find(name, namelen);
  if (tmpCtl != 0x0) {
    kprintf("Node Exists! [%s]\n", str_name);
    while (1)
      ;
  }

  /* Get Parent Node */
  tmpCtl = sysctl_find(name, namelen - 1);
  if (tmpCtl == 0x0) {
    kprintf("Parent Node Non Existant\n");
    return (-1);
  }
  if (tmpCtl->children == 0x0) {
    tmpCtl->children = (struct sysctl_entry *) kmalloc(sizeof(struct sysctl_entry));
    memset(tmpCtl->children, 0x0, sizeof(struct sysctl_entry));
    tmpCtl->children->children = 0x0;
    tmpCtl->children->prev = 0x0;
    tmpCtl->children->next = 0x0;
    tmpCtl->children->id = name[namelen - 1];
    for (i = 0; i < namelen;i++)
      tmpCtl->children->full_name[i] = name[i];

    tmpCtl->children->namelen = namelen;

    sprintf(tmpCtl->children->name, str_name);

    insert_trieNode(&sysctl_headTrie, tmpCtl->children->name, tmpCtl->children);

    tmpCtl->children->value = (void *) kmalloc(buf_size);
    memcpy(tmpCtl->children->value, buf, buf_size);
    tmpCtl->children->val_len = buf_size;
  }
  else {
    newCtl = (struct sysctl_entry *) kmalloc(sizeof(struct sysctl_entry));
    memset(newCtl, 0x0, sizeof(struct sysctl_entry));
    newCtl->prev = 0x0;
    newCtl->next = tmpCtl->children;
    newCtl->children = 0x0;
    newCtl->id = name[namelen - 1];

    for (i = 0; i<namelen;i++)
      newCtl->full_name[i] = name[i];

    newCtl->namelen = namelen;

    sprintf(newCtl->name, str_name);

    insert_trieNode(&sysctl_headTrie, newCtl->name, newCtl);

    newCtl->value = (void *) kmalloc(buf_size);
    memcpy(newCtl->value, buf, buf_size);
    newCtl->val_len = buf_size;
    tmpCtl->children->prev = newCtl;
    tmpCtl->children = newCtl;
  }

  return (0x0);
}

/***
 END
 ***/
