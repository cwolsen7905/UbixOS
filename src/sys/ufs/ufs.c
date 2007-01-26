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

 $Id$

*****************************************************************************************/

#include <vfs/vfs.h>
#include <ufs/ufs.h>
#include <ufs/ffs.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <ubixos/kpanic.h>
#include <lib/string.h>
#include <sys/kern_descrip.h>

#define VBLKSHIFT       12
#define VBLKSIZE        (1 << VBLKSHIFT)
#define VBLKMASK        (VBLKSIZE - 1)
#define DBPERVBLK       (VBLKSIZE / DEV_BSIZE)
#define INDIRPERVBLK(fs) (NINDIR(fs) / ((fs)->fs_bsize >> VBLKSHIFT))
#define IPERVBLK(fs)    (INOPB(fs) / ((fs)->fs_bsize >> VBLKSHIFT))
#define       INOPB(fs)       ((fs)->fs_inopb)
#define INO_TO_VBA(fs, ipervblk, x) \
    (fsbtodb(fs, cgimin(fs, ino_to_cg(fs, x))) + \
    (((x) % (fs)->fs_ipg) / (ipervblk) * DBPERVBLK))
#define INO_TO_VBO(ipervblk, x) ((x) % ipervblk)


static int dskread(void *buf, u_int64_t block,size_t count,struct file *fd) {
  fd->mp->device->devInfo->read(fd->mp->device->devInfo->info,buf,block,count);
  return(0x0);
  }

//struct dmadat {
//  char blkbuf[VBLKSIZE];  /* filesystem blocks */
//  char indbuf[VBLKSIZE];  /* indir blocks */
//  char sbbuf[SBLOCKSIZE]; /* superblock */
//  char secbuf[DEV_BSIZE]; /* for MBR/disklabel */
//  };
//static struct dmadat *dmadat;

//static int ls,dsk_meta;

static int sblock_try[] = SBLOCKSEARCH;

#if defined(UFS2_ONLY)
#define DIP(field) dp2.field
#elif defined(UFS1_ONLY)
#define DIP(field) dp1.field
#else
#define DIP(field) fs->fs_magic == FS_UFS1_MAGIC ? dp1.field : dp2.field
#endif


static ssize_t fsread(ino_t inode, void *buf, size_t nbyte,struct file *fd) {
#ifndef UFS2_ONLY
        static struct ufs1_dinode dp1;
#endif
#ifndef UFS1_ONLY
        static struct ufs2_dinode dp2;
#endif
        static ino_t inomap;
        char *blkbuf;
        void *indbuf;
        struct fs *fs;
        char *s;
        size_t n, nb, size, off, vboff;
        ufs_lbn_t lbn;
        ufs2_daddr_t addr, vbaddr;
        static ufs2_daddr_t blkmap, indmap;
        u_int u;
        struct ufs_obj *ufsObj = fd->fsObj;

        blkbuf = ufsObj->dmadat->blkbuf;
        indbuf = ufsObj->dmadat->indbuf;
        fs = (struct fs *)ufsObj->dmadat->sbbuf;

#ifdef DEBUG
kprintf("fsread!\n");
#endif

        if (!ufsObj->dsk_meta) {
                inomap = 0;
                for (n = 0; sblock_try[n] != -1; n++) {
                        if (dskread(fs, sblock_try[n] / DEV_BSIZE, 16,fd))
                                return -1;
                        if ((
#if defined(UFS1_ONLY)
                             fs->fs_magic == FS_UFS1_MAGIC
#elif defined(UFS2_ONLY)
                            (fs->fs_magic == FS_UFS2_MAGIC &&
                            fs->fs_sblockloc == sblock_try[n])
#else
                             fs->fs_magic == FS_UFS1_MAGIC ||
                            (fs->fs_magic == FS_UFS2_MAGIC &&
                            fs->fs_sblockloc == sblock_try[n])
#endif
                            ) &&
                            fs->fs_bsize <= MAXBSIZE &&
                            fs->fs_bsize >= sizeof(struct fs))
                                break;
                }
                if (sblock_try[n] == -1) {
                        kprintf("Not ufs\n");
                        return -1;
                }
                ufsObj->dsk_meta++;
        }

  if (!inode) {
#ifdef DEBUG
    kprintf("!node\n");
#endif
    return(0x0);
    }

        if (inomap != inode) {
#ifdef DEBUG
   kprintf("inomap != inode\n");
#endif
                n = IPERVBLK(fs);
                if (dskread(blkbuf, INO_TO_VBA(fs, n, inode), DBPERVBLK,fd))
                        return -1;
                n = INO_TO_VBO(n, inode);
#if defined(UFS1_ONLY)
                dp1 = ((struct ufs1_dinode *)blkbuf)[n];
#elif defined(UFS2_ONLY)
                dp2 = ((struct ufs2_dinode *)blkbuf)[n];
#else
                if (fs->fs_magic == FS_UFS1_MAGIC)
                        dp1 = ((struct ufs1_dinode *)blkbuf)[n];
                else
                        dp2 = ((struct ufs2_dinode *)blkbuf)[n];
#endif
                inomap = inode;
                fd->offset = 0;
                blkmap = indmap = 0;
        }

        s = buf;
        size = DIP(di_size);
        fd->size = size;
        n = size - fd->offset;
#ifdef DEBUG
  kprintf("n: [0x%X:0x%X:0x%X]\n",n,fd->offset,size);
#endif
 //Why?
        if (n < 0)
          return(0x0);
        if (nbyte > n)
                nbyte = n;
        nb = nbyte;
        while (nb) {
                lbn = lblkno(fs, fd->offset);
                off = blkoff(fs, fd->offset);
                if (lbn < NDADDR) {
                        addr = DIP(di_db[lbn]);
                } else if (lbn < NDADDR + NINDIR(fs)) {
                        n = INDIRPERVBLK(fs);
                        addr = DIP(di_ib[0]);
                        u = (u_int)(lbn - NDADDR) / (n * DBPERVBLK);
                        vbaddr = fsbtodb(fs, addr) + u;
                        if (indmap != vbaddr) {
                                if (dskread(indbuf, vbaddr, DBPERVBLK,fd))
                                        return -1;
                                indmap = vbaddr;
                        }
                        n = (lbn - NDADDR) & (n - 1);
#if defined(UFS1_ONLY)
                        addr = ((ufs1_daddr_t *)indbuf)[n];
#elif defined(UFS2_ONLY)
                        addr = ((ufs2_daddr_t *)indbuf)[n];
#else
                        if (fs->fs_magic == FS_UFS1_MAGIC)
                                addr = ((ufs1_daddr_t *)indbuf)[n];
                        else
                                addr = ((ufs2_daddr_t *)indbuf)[n];
#endif
                } else {
                        return -1;
                }
                vbaddr = fsbtodb(fs, addr) + (off >> VBLKSHIFT) * DBPERVBLK;
                vboff = off & VBLKMASK;
                n = sblksize(fs, size, lbn) - (off & ~VBLKMASK);
                if (n > VBLKSIZE)
                        n = VBLKSIZE;
                if (blkmap != vbaddr) {
                        if (dskread(blkbuf, vbaddr, n >> DEV_BSHIFT,fd))
                                return -1;
                        blkmap = vbaddr;
                }
                n -= vboff;
                if (n > nb)
                        n = nb;
                memcpy(s, blkbuf + vboff, n);
                s += n;
                fd->offset += n;
                nb -= n;
        }
        return nbyte;
}




static __inline int fsfind(const char *name, ino_t * ino,struct file *fd) {
  char buf[DEV_BSIZE];
  struct dirent *d;
  char *s;
  ssize_t n;

  fd->offset = 0;
  while ((n = fsread(*ino, buf, DEV_BSIZE,fd)) > 0)
    for (s = buf; s < buf + DEV_BSIZE;) {
      d = (void *)s;
      if (!strcmp(name, d->d_name)) {
        *ino = d->d_fileno;
        return d->d_type;
        }
      s += d->d_reclen;
      }
    //if (n != -1 && ls)
    //kprintf("\n");
    return 0;
    }


static ino_t lookup(const char *path,struct file *fd) {
  char name[MAXNAMLEN + 1];
        const char *s;
        ino_t ino;
        ssize_t n;
        int dt;

        ino = ROOTINO;
        dt = DT_DIR;
        name[0] = '/';
        name[1] = '\0';
        for (;;) {
                if (*path == '/')
                        path++;
                if (!*path)
                        break;
                for (s = path; *s && *s != '/'; s++);
                if ((n = s - path) > MAXNAMLEN)
                        return 0;
                //ls = *path == '?' && n == 1 && !*s;
                memcpy(name, path, n);
                name[n] = 0;
                if (dt != DT_DIR) {
                        kprintf("%s: not a directory.\n", name);
                        return (0);
                }
                if ((dt = fsfind(name, &ino,fd)) <= 0)
                        break;
                path = s;
        }


  return dt == DT_REG ? ino : 0;
  }


static int ufs_openFile(const char *file,struct file *fd) {
  char tmp[2];
  int ino = 0;
  struct ufs_obj *ufsObj = 0x0;

  ufsObj = (struct ufs_obj *)kmalloc(sizeof(struct ufs_obj));
  fd->fsObj = ufsObj;

  ufsObj->dmadat = (struct dmadat *)kmalloc(sizeof(struct dmadat));
  ino = lookup(file,fd);
  fd->offset = 0x0;
  ufsObj->ino = ino;
  if (ino == 0x0) {
    return(-1);
    }

  /* Quick Hack for file size */
  fsread(ufsObj->ino,&tmp,1,fd);
  fd->offset = 0;
  /* Return */
  fd->perms = 0x1;
  return(0x1);
  }

int ufs_readFile(struct file *fd,char *data,uInt32 offset,long size) {
  struct ufs_obj *ufsObj = fd->fsObj;
  return(fsread(ufsObj->ino,data,size,fd));
  }

int ufs_writeFile(fileDescriptor *fd, char *data,uInt32 offset,long size) {
  kprintf("Writing :)\n");
  return(0x0);
  }

/*****************************************************************************************

Function: int ufs_initialize()

Description: This will initialize a mount point it loads the BAT and Caches the rootDir

Notes:

*****************************************************************************************/
int ufs_initialize(struct vfs_mountPoint *mp) {
  /* Return */
  return(0x1);
  }

int ufs_init() {
  /* Build our ufs struct */
  struct fileSystem ufs =
   {NULL,                         /* prev        */
    NULL,                         /* next        */
    (void *)ufs_initialize,       /* vfsInitFS   */
    (void *)ufs_readFile,         /* vfsRead     */
    (void *)ufs_writeFile,        /* vfsWrite    */
    (void *)ufs_openFile,         /* vfsOpenFile */
    NULL,                         /* vfsUnlink   */
    NULL,                         /* vfsMakeDir  */
    NULL,                         /* vfsRemDir   */
    NULL,                         /* vfsSync     */
    0xAA,                         /* vfsType     */
   }; /* UFS */

  if (vfsRegisterFS(ufs) != 0x0) {
    kpanic("Unable To Enable UFS");
    return(0x1);
    }
   //dmadat = (struct dmadat *)kmalloc(sizeof(struct dmadat)); 
  /* Return */
  return(0x0);
  }

/***
 END
 ***/

