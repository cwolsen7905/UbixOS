#include <vfs/vfs.h>
#include <ubixos/wait.h>

static struct inode *first_inode;
static struct wait_queue *inode_wait = NULL;
static int nr_inodes = 0, nr_free_inodes = 0;

void iput(struct inode * inode) {
  if (!inode)
    return;
  wait_on_inode(inode);

#ifdef _BALLS
  if (!inode->i_count) {
    printk("VFS: iput: trying to free free inode\n");
    printk("VFS: device %d/%d, inode %lu, mode=0%07o\n", MAJOR(inode->i_rdev), MINOR(inode->i_rdev), inode->i_ino, inode->i_mode);
    return;
  }
#endif

  if (inode->i_pipe)
    wake_up_interruptible(&PIPE_WAIT(*inode));
  repeat: if (inode->i_count > 1) {
    inode->i_count--;
    return;
  }
  wake_up(&inode_wait);
  if (inode->i_pipe) {
    unsigned long page = (unsigned long) PIPE_BASE(*inode);
    PIPE_BASE (*inode) = NULL;
    free_page(page);
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
