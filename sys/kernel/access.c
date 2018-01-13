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

#include <ubixos/access.h>

/************************************************************************

 Function: sys_setUID();

 Description: This will set the UID of the task

 Notes:

 2016/01/17 - This is very basic and will set any UID if you're 0
 need to change later.


 ************************************************************************/
int sys_setUID(struct thread *td, struct sys_setUID_args *args) {
  if (_current->uid == 0x0) {
    _current->uid = args->uid;
    return (0);
  }
  return (-1);
}

int sys_getUID(struct thread *td, void *uap) {
  return (_current->uid);
}

int sys_getEUID(struct thread *td, void *uap) {
  return (_current->uid);
}

int sys_getGID(struct thread *td, void *uap) {
  return (_current->gid);
}

int sys_setGID(struct thread *td, struct sys_setGID_args *uap) {

  if (_current->gid == 0x0) {

    _current->gid = uap->gid;

    return (0);

  }

  return (-1);
}

int in_group_p(gid_t grp) {
  int i;

  if (grp == _current->egid)
    return 1;

  for (i = 0; i < NR_GROUPS; i++) {
    if (_current->groups[i] == NO_GROUP)
      break;
    if (_current->groups[i] == grp)
      return 1;
  }
  return 0;
}
