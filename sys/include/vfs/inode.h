#ifndef _VFS_INODE_H
#define _VFS_INODE_H

#include <sys/types.h>
#include <ubixos/wait.h>

#include <fs/pipe_fs.h>
#include <fs/msdos_fs.h>
#include <ufs/ufs.h>

struct inode {
    __dev_t i_dev;
    unsigned long i_ino;
    __mode_t i_mode;
    __nlink_t i_nlink;
    uid_t i_uid;
    gid_t i_gid;
    __dev_t i_rdev;
    off_t i_size;
    time_t i_atime;
    time_t i_mtime;
    time_t i_ctime;
    unsigned long i_blksize;
    unsigned long i_blocks;
    struct semaphore i_sem;
    struct inode_operations * i_op;
    struct super_block * i_sb;
    struct wait_queue * i_wait;
    struct file_lock * i_flock;
    struct vm_area_struct * i_mmap;
    struct inode * i_next, *i_prev;
    struct inode * i_hash_next, *i_hash_prev;
    struct inode * i_bound_to, *i_bound_by;
    struct inode * i_mount;
    struct socket * i_socket;
    unsigned short i_count;
    unsigned short i_flags;
    unsigned char i_lock;
    unsigned char i_dirt;
    unsigned char i_pipe;
    unsigned char i_seek;
    unsigned char i_update;
    union {
        struct pipe_inode_info pipe_i;
        struct msdos_inode_info msdos_i;
        struct ufs1_dinode ufs1_i;
        struct ufs2_dinode ufs2_i;
    } u;
};


#endif
