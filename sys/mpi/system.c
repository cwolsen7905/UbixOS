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

#include <mpi/mpi.h>
#include <lib/kmalloc.h>
#include <lib/string.h>
#include <ubixos/spinlock.h>

static mpi_mbox_t *mboxList = 0x0;
static struct spinLock mpiSpinLock = SPIN_LOCK_INITIALIZER;

/*****************************************************************************************

 Function: static mpiMbox_t * mpiFindMbox(char *name)

 Description: This function will find a mail box that matches the supplied name

 Notes: This function is not task-safe!  Lock must be done before call.

 *****************************************************************************************/
static mpi_mbox_t * mpi_findMbox(char *name) {
  mpi_mbox_t *mbox = 0x0;

  for (mbox = mboxList; mbox; mbox = mbox->next) {
    if (!strcmp(mbox->name, name)) {
      return (mbox);
    }
  }

  return (0x0);
}

/*****************************************************************************************

 Function: int mpiCreateMbox(char *name)

 Description: This function will create a new mailbox if it fails it will return -1
 otherwise it returns 0x0

 Notes:

 *****************************************************************************************/
int mpi_createMbox(char *name) {
  mpi_mbox_t *mbox = 0x0;

  spinLock(&mpiSpinLock);
  if (mpi_findMbox(name) != 0x0) {
    spinUnlock(&mpiSpinLock);
    return (-1);
  }

  mbox = (mpi_mbox_t *) kmalloc(sizeof(mpi_mbox_t));

  sprintf(mbox->name, name);
  mbox->pid = _current->id;

  if (mboxList == 0x0) {
    mbox->prev = 0x0;
    mbox->next = 0x0;
    mboxList = mbox;
  }
  else {
    mbox->next = mboxList;
    mbox->prev = 0x0;
    mboxList->prev = mbox;
    mboxList = mbox;
  }

  spinUnlock(&mpiSpinLock);
  return (0x0);
}

/*****************************************************************************************

 Function: int mpiSpam(uInt32 type,void *data)

 Description: This function will send a message to every mailbox

 Notes:

 *****************************************************************************************/
int mpi_spam(uInt32 type, void *data) {
  mpi_mbox_t *mbox = 0x0;
  mpi_message_t *message = 0x0;

  spinLock(&mpiSpinLock);

  for (mbox = mboxList; mbox; mbox = mbox->next) {
    message = (mpi_message_t *) kmalloc(sizeof(mpi_message_t));

    message->header = type;
    memcpy(message->data, data, MESSAGE_LENGTH);
    message->next = 0x0;

    if (mbox->msg == 0x0) {
      mbox->msg = message;
    }
    else {
      mbox->msgLast->next = message;
      mbox->msgLast = message;
    }
  }

  spinUnlock(&mpiSpinLock);
  return (0x0);
}

/*****************************************************************************************

 Function: int mpiPostMessage(char *name,uInt32 type,void *data)

 Description: This function will post a message to specified mailbox

 Notes:

 *****************************************************************************************/
int mpi_postMessage(char *name, uInt32 type, mpi_message_t *msg) {
  mpi_mbox_t *mbox = 0x0;
  mpi_message_t *message = 0x0;

  spinLock(&mpiSpinLock);

  mbox = mpi_findMbox(name);

  if (mbox == 0x0) {
    spinUnlock(&mpiSpinLock);
    return (0x1);
  }

  message = (mpi_message_t *) kmalloc(sizeof(mpi_message_t));

  message->header = msg->header;
  memcpy(message->data, msg->data, MESSAGE_LENGTH);
  message->pid = _current->id;
  message->next = 0x0;

  if (mbox->msg == 0x0) {
    mbox->msg = message;
  }
  else {
    mbox->msgLast->next = message;
    mbox->msgLast = message;
  }

  spinUnlock(&mpiSpinLock);

  if (type == 0x2) {
    while (mbox->msgLast != 0x0)
      ;
  }

  return (0x0);
}

/*****************************************************************************************

 Function: int mpiFetchMessage(char *name,mpiMessage_t *msg)

 Description: This function will fetch the next message out of the specified mailbox

 Notes:

 *****************************************************************************************/
int mpi_fetchMessage(char *name, mpi_message_t *msg) {
  mpi_mbox_t *mbox = 0x0;
  mpi_message_t *tmpMsg = 0x0;

  spinLock(&mpiSpinLock);

  mbox = mpi_findMbox(name);

  if (mbox == 0x0) {
    spinUnlock(&mpiSpinLock);
    return (-1);
  }

  if (mbox->msg == 0x0) {
    spinUnlock(&mpiSpinLock);
    return (-1);
  }

  if (mbox->pid != _current->id) {
    spinUnlock(&mpiSpinLock);
    return (-1);
  }

  msg->header = mbox->msg->header;
  memcpy(msg->data, mbox->msg->data, MESSAGE_LENGTH);
  msg->pid = mbox->msg->pid;

  tmpMsg = mbox->msg;
  mbox->msg = mbox->msg->next;

  kfree(tmpMsg);

  spinUnlock(&mpiSpinLock);
  return (0x0);
}

/*****************************************************************************************

 Function: int mpiDestroyMbox(char *name)

 Description: This function will fetch the next message out of the specified mailbox

 Notes:

 *****************************************************************************************/
int mpi_destroyMbox(char *name) {
  mpi_mbox_t *mbox = 0x0;

  spinLock(&mpiSpinLock);

  for (mbox = mboxList; mbox; mbox = mbox->next) {
    if (!strcmp(mbox->name, name)) {
      if (mbox->pid != _current->id) {
        spinUnlock(&mpiSpinLock);
        return (-1);
      }
      mbox->prev->next = mbox->next;
      mbox->next->prev = mbox->prev;
      kfree(mbox);
      spinUnlock(&mpiSpinLock);
      return (0x0);
    }
  }

  spinUnlock(&mpiSpinLock);
  return (-1);
}

/***
 END
 ***/
