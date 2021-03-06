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

#include <vfs/vfs.h>
#include <ubixos/wait.h>
#include <ubixos/sched.h>

//static struct inode *first_inode = NULL;
static struct wait_queue *inode_wait = NULL;
//static int nr_inodes = 0;
static int nr_free_inodes = 0;

static void write_inode(struct inode * inode);
static void __wait_on_inode(struct inode * inode);
static inline void unlock_inode(struct inode * inode);

static inline void wait_on_inode(struct inode * inode) {
  if (inode->i_lock)
    __wait_on_inode(inode);
}

void iput(struct inode * inode) {
  if (!inode)
    return;

  wait_on_inode(inode);

#ifdef _IGNORE
  if (!inode->i_count) {
    printk("VFS: iput: trying to free free inode\n");
    printk("VFS: device %d/%d, inode %lu, mode=0%07o\n", MAJOR(inode->i_rdev), MINOR(inode->i_rdev), inode->i_ino, inode->i_mode);
    return;
  }
#endif

  if (inode->i_pipe)
    wake_up_interruptible(&PIPE_WAIT(*inode));

  repeat:

  if (inode->i_count > 1) {
    inode->i_count--;
    return;
  }

  wake_up(&inode_wait);

  if (inode->i_pipe) {
    unsigned long page = (unsigned long) PIPE_BASE(*inode);
    PIPE_BASE (*inode) = NULL;
    vmm_freeVirtualPage(page);
  }

  if (inode->i_sb && inode->i_sb->s_op && inode->i_sb->s_op->put_inode) {
    inode->i_sb->s_op->put_inode(inode);
    if (!inode->i_nlink)
      return;
  }

  if (inode->i_dirt) {
    write_inode(inode); /* we can sleep - so do again */
    wait_on_inode(inode);
    goto repeat;
  }

  inode->i_count--;
  nr_free_inodes++;
  return;
}

static void __wait_on_inode(struct inode * inode) {
  struct wait_queue wait = { _current, NULL };

  add_wait_queue(&inode->i_wait, &wait);

  repeat:

  _current->state = UNINTERRUPTIBLE;

  if (inode->i_lock) {
    sched_yield();
    //schedule();
    goto repeat;
  }

  remove_wait_queue(&inode->i_wait, &wait);

  _current->state = RUNNING;

}

static void write_inode(struct inode * inode) {
  if (!inode->i_dirt)
    return;

  wait_on_inode(inode);

  if (!inode->i_dirt)
    return;

  if (!inode->i_sb || !inode->i_sb->s_op || !inode->i_sb->s_op->write_inode) {
    inode->i_dirt = 0;
    return;
  }

  inode->i_lock = 1;
  inode->i_sb->s_op->write_inode(inode);

  unlock_inode(inode);
}


static inline void unlock_inode(struct inode * inode) {
  inode->i_lock = 0;
  wake_up(&inode->i_wait);
}
