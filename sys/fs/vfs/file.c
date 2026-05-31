/*-
 * Copyright (c) 2002-2018, 2020 The UbixOS Project.
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
#include <sys/sysproto_posix.h>
#include <sys/sysproto.h>
#include <fs/vfs/vfs.h>
#include <fs/fat/fat_file.h>
#include <ubixos/vitals.h>
#include <ubixos/kpanic.h>
#include <ubixos/spinlock.h>
#include <lib/kmalloc.h>
#include <string.h>
#include <vmm/paging.h>
#include <lib/kprintf.h>
#include <assert.h>
#include <sys/klog.h>
#include <sys/errno.h>
#include <sys/descrip.h>
#include <sys/pipe.h>
/* fat_filelib.h removed — FAT now uses native driver via vfsClose */

static struct spinLock fdTable_lock = SPIN_LOCK_INITIALIZER
;

int sysMkDir(const char *path);

fileDescriptor_t *fdTable = 0x0;

fileDescriptor_t *vfs_fileTable = 0x0;

int sys_fwrite(struct thread *td, struct sys_fwrite_args *uap) {
    char *t = uap->buf;

    if (uap->fd == 0x0) {
        tty_print((char*) uap->buf, _current->term);
    }
    else {

#ifdef DEBUG_VFS
    kprintf("uap->size: %i, FD: [0x%X], BUF: [0x%X][%c]\n", uap->nbytes, uap->fd, uap->buf, t[0]);
#endif

        fwrite(uap->buf, uap->nbytes, 1, uap->fd->fd);
    }

    td->td_retval[0] = 0x0;

    return (0);
}

/* USER */

void sysFwrite(char *ptr, int size, userFileDescriptor *userFd) {
    if (userFd == 0x0) {
        tty_print(ptr, _current->term);
    }
    else {
        fwrite(ptr, size, 1, userFd->fd);
    }
    return;
}

int sys_fgetc(struct thread *td, struct sys_fgetc_args *args) {
    char c;

    if (args->FILE->fd == 0x0) {
        /* stdin: check if it has been redirected to a pipe */
        struct file *stdinf = (struct file *)td->o_files[0];
        if (stdinf != 0x0 && stdinf->fd_type == 3) {
            /* pipe-backed stdin: block until a byte arrives */
            struct pipeInfo *pFD = (struct pipeInfo *)stdinf->data;
            while (pFD->bCNT == 0)
                sched_yield();
            /* read one byte from the pipe */
            struct pipeBuf *pb = pFD->headPB;
            c = pb->buffer[pb->offset++];
            if (pb->offset >= pb->nbytes) {
                pFD->headPB = pb->next;
                kfree(pb->buffer);
                kfree(pb);
                pFD->bCNT--;
            }
            td->td_retval[0] = (unsigned char)c;
            return (0);
        }

        /* TTY-backed stdin */
        while (1) {
            if (_current->term == tty_foreground) {
                c = getchar();
                if (c != 0x0) {
                    td->td_retval[0] = c;
                    return (0);
                }
                sched_yield();
            }
            else {
                sched_yield();
            }
        }
    }
    else {
        c = fgetc(args->FILE->fd);
        td->td_retval[0] = c;
        return (0);
    }
}

void sysRmDir(const char *path) {
    char fullpath[1024];
    const char *dir_path = 0x0;
    struct vfs_mountPoint *mp = 0x0;

    if (path[0] != '/')
        snprintf(fullpath, sizeof(fullpath), "%s%s", _current->oInfo.cwd, path);
    else
        strncpy(fullpath, path, sizeof(fullpath) - 1);
    fullpath[sizeof(fullpath) - 1] = '\0';

    mp = vfs_findMount(fullpath);
    if (mp == 0x0 || mp->fs == 0x0 || mp->fs->vfsRemDir == 0x0)
        return;

    size_t mlen = strlen(mp->mountPoint);
    dir_path = (mlen > 1) ? fullpath + mlen : fullpath;
    if (dir_path[0] == '\0') dir_path = "/";

    mp->fs->vfsRemDir(dir_path, mp);
}

int sys_mkdir(struct thread *td, struct sys_mkdir_args *args) {
  if (args->path == 0x0) {
    td->td_retval[0] = -EFAULT;
    return (EFAULT);
  }
  /* sysMkDir returns 0 on success, -EEXIST when the path already names
   * a directory, -EIO on filesystem failure.  busybox's mkdir -p relies
   * on EEXIST being distinguishable so it can walk through interior
   * components that already exist. */
  int r = sysMkDir(args->path);
  td->td_retval[0] = r;
  return (r == 0 ? 0 : -r);
}

int sys_rmdir(struct thread *td, struct sys_rmdir_args *args) {
  if (args->path == 0x0) {
    td->td_retval[0] = -1;
    return (-1);
  }
  sysRmDir(args->path);
  td->td_retval[0] = 0;
  return (0);
}

int sys_fseek(struct thread *td, struct sys_fseek_args *args) {
    // TODO : coredump?
    if (args->FILE == NULL) {
        td->td_retval[0] = -1;
        return (-1);
    }

    if (args->FILE->fd == NULL) {
        td->td_retval[0] = -1;
        return (-1);
    }

    switch (args->whence) {
        case 0:
            args->FILE->fd->offset = args->offset;
            break;
        case 1:
            args->FILE->fd->offset += args->offset;
            break;
        default:
            kprintf("seek-whence: %i", args->whence);
            break;
    }

    td->td_retval[0] = args->FILE->fd->offset & 0xFFFFFFFF;
    return (0);
}

int sys_lseek(struct thread *td, struct sys_lseek_args *args) {
    struct file *fdd = 0x0;
    fileDescriptor_t *fd = 0x0;
    off_t newpos = 0;

    getfd(td, &fdd, args->fd);

    if (fdd == 0x0 || fdd->fd == 0x0) {
        td->td_retval[0] = -1;
        return (-1);
    }

    fd = fdd->fd;

    switch (args->whence) {
        case SEEK_SET:
            fd->offset = args->offset;
            break;
        case SEEK_CUR:
            fd->offset += args->offset;
            break;
        case SEEK_END:
            fd->offset = (off_t)fd->size + args->offset;
            break;
        default:
            kprintf("lseek: unknown whence %d\n", args->whence);
            td->td_retval[0] = -1;
            return (-1);
    }

    if (fd->offset < 0)
        fd->offset = 0;

    newpos = fd->offset;
    td->td_retval[0] = (int32_t)(newpos & 0xFFFFFFFF);
    td->td_retval[1] = (int32_t)(newpos >> 32);
    return (0);
}

/**
 * sys_ftruncate — POSIX ftruncate(2), FreeBSD ABI syscall 480.
 *
 * Resizes the on-disk file plus the in-kernel tracked size.  Shrinking
 * frees the cluster chain beyond the new length and updates the FAT
 * directory entry; growing produces a sparse-style file (size bumped,
 * gap filled by subsequent writes).
 *
 * Implementation currently calls fat_file_truncate directly via fd->res —
 * UbixOS's only writable filesystem is FAT, and we don't yet have a
 * per-FS truncate hook in the vfs op table.  When a second writable FS
 * lands we should switch to dispatching through the mount's fs vector.
 *
 * @return 0 on success, -EBADF if fd does not refer to an open writable
 *         file, -EINVAL for negative length, -EIO on FS failure.
 */
int sys_ftruncate(struct thread *td, struct sys_ftruncate_args *args) {
    struct file *fdd = 0x0;
    fileDescriptor_t *fd = 0x0;

    getfd(td, &fdd, args->fd);
    if (fdd == 0x0 || fdd->fd == 0x0) {
        td->td_retval[0] = -EBADF;
        return (EBADF);
    }
    fd = fdd->fd;

    if (args->length < 0) {
        td->td_retval[0] = -EINVAL;
        return (EINVAL);
    }

    /* FAT: ask the driver to resize the chain.  Other FSes (ufs/ubixfs)
     * land in the in-memory-only fallback below since they're read-only
     * for our current workloads. */
    if (fd->res != NULL && fd->mp != NULL && fd->mp->fs != NULL &&
        fd->mp->fs->vfsType == 0xFA) {
        if (fat_file_truncate((struct fat_file *)fd->res,
                              (u_int32_t)args->length) != 0) {
            td->td_retval[0] = -EIO;
            return (EIO);
        }
    }

    fd->size = (u_int32_t)args->length;
    if (fd->offset > args->length)
        fd->offset = args->length;

    td->td_retval[0] = 0;
    return (0);
}

/**
 * sys_umask — POSIX umask(2), FreeBSD ABI syscall 60.
 *
 * Returns the old umask and sets the new one.  The per-process umask
 * isn't yet applied at file-create time (kern_openat doesn't consult
 * it), but tools that pass through "save and restore umask" — mkdir,
 * touch, tar — call it during init and would fail outright if the
 * syscall returned -ENOSYS.  Stubbing to 022 / accept-anything keeps
 * those callers happy until per-process umask tracking lands.
 *
 * @return previous umask (0022 fixed for now).
 */
int sys_umask(struct thread *td, struct sys_umask_args *args) {
    (void)args;
    td->td_retval[0] = 0022;
    return (0);
}

int sys_chdir(struct thread *td, struct sys_chdir_args *args) {
    char newcwd[1024];
    size_t len;
    kDIR_t *dir = 0x0;

    /* Build the candidate path without touching cwd yet.
     * Absolute path: use as-is.  Relative path: prepend CWD. */
    if (args->path[0] == '/') {
        snprintf(newcwd, sizeof(newcwd), "%s", args->path);
    } else {
        snprintf(newcwd, sizeof(newcwd), "%s%s", _current->oInfo.cwd, args->path);
    }

    /* ensure trailing '/' */
    len = strlen(newcwd);
    if (len > 0 && newcwd[len - 1] != '/' && len + 1 < sizeof(newcwd)) {
        newcwd[len]     = '/';
        newcwd[len + 1] = '\0';
    }

    /* Validate the path exists as a directory */
    dir = vfs_opendir(newcwd);
    if (dir == 0x0) {
        td->td_retval[0] = -1;
        return (-1);
    }
    vfs_closedir(dir);

    memcpy(_current->oInfo.cwd, newcwd, sizeof(newcwd));
    td->td_retval[0] = 0;
    return (0);
}

int sys_fchdir(struct thread *td, struct sys_fchdir_args *args) {
    int error = 0;
    struct file *fdd = 0x0;
    fileDescriptor_t *fd = 0x0;

    getfd(td, &fdd, args->fd);

    if (fdd == 0x0 || fdd->fd == 0x0) {
        td->td_retval[0] = -1;
        return (-1);
    }

    fd = fdd->fd;

    snprintf(_current->oInfo.cwd, sizeof(_current->oInfo.cwd), "%s", fd->fileName);
    return (error);
}

int sys_rename(struct thread *td, struct sys_rename_args *args) {
    td->td_retval[0] = 0;
    return (0);
}

int sysUnlink(const char *path, int *retVal) {
    *retVal = 0;
    return (*retVal);
}

int sys_ttyctrl(struct thread *td, struct sys_ttyctrl_args *args) {
    tty_term *t = _current->term;
    if (t == NULL) { td->td_retval[0] = -1; return (-1); }
    switch (args->cmd) {
        case TTY_SETRAW:  t->t_raw  = args->val ? 1 : 0; break;
        case TTY_SETECHO: t->t_echo = args->val ? 1 : 0; break;
        default: td->td_retval[0] = -1; return (-1);
    }
    td->td_retval[0] = 0;
    return (0);
}

/************************************************************************

 Function: void sysFopen();
 Description: Opens A File Descriptor For A User Task
 Notes:

 ************************************************************************/
//void sysFopen(const char *file,char *flags,userFileDescriptor *userFd) {
int sys_fopen(struct thread *td, struct sys_fopen_args *args) {
    if (args->FILE == NULL) {
        kprintf("Error: userFd == NULL, File: %s, Line: %i\n", __FILE__, __LINE__);
        return (-1);
    }

    args->FILE->fd = fopen(args->path, args->mode);
    if (args->FILE->fd != 0x0) {
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
int sys_fread(struct thread *td, struct sys_fread_args *args) {

    /* TODO : coredump? */
    if (args->FILE == NULL)
        return (-1);

    if (args->FILE->fd == NULL)
        return (-1);

    td->td_retval[0] = fread(args->ptr, args->size, args->nmemb, args->FILE->fd);
    return (0);
}

/************************************************************************

 Function: void sysFclse();
 Description: Closes A File Descriptor For A User Task
 Notes:

 ************************************************************************/
int sys_fclose(struct thread *td, struct sys_fclose_args *args) {
    if (args->FILE == NULL) {
        return (-1);
    }

    /* Return */
    return (fclose(args->FILE->fd));
}

/* KERNEL */

size_t fread(void *ptr, size_t size, size_t nmemb, fileDescriptor_t *fd) {
    size_t i = 0x0;

    if (fd == 0x0)
        return (0x0);

    if (nmemb == 0x0)
        nmemb = 1;  //Temp Fix

    assert(fd);
    assert(fd->mp);
    assert(fd->mp->fs);

    i = fd->mp->fs->vfsRead(fd, ptr, fd->offset, size * nmemb);

    fd->offset += i;

    return (i);
}

size_t fwrite(void *ptr, int size, int nmemb, fileDescriptor_t *fd) {
    int res = 0x0;
    /*
     kprintf("fd[0x%X]\m", fd);
     kprintf("fd->mp[0x%X]\m", fd->mp);
     kprintf("fd->mp->fs[0x%X]\m", fd->mp->fs);
     */

    if (fd != 0x0 && fd->mp->fs->vfsWrite != NULL) {
        res = fd->mp->fs->vfsWrite(fd, ptr, fd->offset, size * nmemb);
        fd->offset += size * nmemb;
    }
    return (res);
}

int kern_fseek(fileDescriptor_t *tmpFd, u_int32_t offset, int whence) {
    switch (whence) {
        case 0: /* SEEK_SET */
            tmpFd->offset = offset;
            break;
        case 1: /* SEEK_CUR */
            tmpFd->offset += offset;
            break;
        default:
            kprintf("kern_fseek: unsupported whence %i\n", whence);
            break;
    }
    return (tmpFd->offset);
}

/************************************************************************

 Function: int feof(fileDescriptor_t *fd)
 Description: Check A File Descriptor For EOF And Return Result
 Notes:

 ************************************************************************/
int feof(fileDescriptor_t *fd) {
    if (fd->status == fdEof) {
        return (-1);
    }
    return (0);
}

/************************************************************************

 Function: int fputc(int ch,fileDescriptor_t *fd)
 Description: This Will Write Character To FD
 Notes:

 ************************************************************************/
int fputc(int ch, fileDescriptor_t *fd) {
    if (fd != 0x0 && fd->mp->fs->vfsWrite != NULL) {
        char c = (char) ch;
        fd->mp->fs->vfsWrite(fd, &c, fd->offset, 1);
        fd->offset++;
        return (unsigned char) ch;
    }
    /* Return NULL If FD Is Not Found */
    return (0x0);
}

/************************************************************************

 Function: int fgetc(fileDescriptor_T *fd)
 Description: This Will Return The Next Character In A FD Stream
 Notes:

 ************************************************************************/
int fgetc(fileDescriptor_t *fd) {
    unsigned char ch = 0x0;
    if (fd != 0x0) {
        if (fd->mp->fs->vfsRead(fd, (char *) &ch, fd->offset, 1) == 0)
            return (-1); /* EOF */
        fd->offset++;
        return (int) ch;
    }
    return (-1);
}

/************************************************************************

 Function: fileDescriptor_t *fopen(const char *file,cont char *flags)
 Description: This Will Open A File And Return A File Descriptor
 Notes:

 08/05/02 - Just Started A Rewrite Of This Function Should Work Out Well

 ************************************************************************/

fileDescriptor_t* fopen(const char *file, const char *flags) {

    int i = 0x0;
    const char *path = 0x0;
    char fileName[1024];
    fileDescriptor_t *tmpFd = 0x0;

    /* Allocate Memory For File Descriptor */
    if ((tmpFd = (fileDescriptor_t*) kmalloc(sizeof(fileDescriptor_t))) == 0x0) {
        kprintf("Error: tmpFd == NULL, File: %s, Line: %i\n", __FILE__, __LINE__);
        return (NULL);
    }

    memset(tmpFd, 0x0, sizeof(fileDescriptor_t));

    if (file == 0x0 || file[0] == '\0') {
        kfree(tmpFd);
        return (NULL);
    }

    /* Resolve path: '.' expands to CWD; everything else is taken as-is. */
    if (file[0] == '.' && file[1] == '\0') {
        strncpy(fileName, _current->oInfo.cwd, sizeof(fileName) - 1);
    } else {
        strncpy(fileName, file, sizeof(fileName) - 1);
    }
    fileName[sizeof(fileName) - 1] = '\0';
    path = fileName;

    /* Find mount by longest POSIX prefix match. */
    tmpFd->mp = vfs_findMount(path);
    if (tmpFd->mp == 0x0) {
        kfree(tmpFd);
        return (0x0);
    }

    /* Compute FS-relative path by stripping the mount prefix.
     * Root mount "/" passes the full path through unchanged. */
    {
        size_t mlen = strlen(tmpFd->mp->mountPoint);
        if (mlen > 1) {
            path = fileName + mlen;
            if (path[0] == '\0')
                path = "/";
        }
    }

    if (path[0] == '/')
        strncpy(tmpFd->fileName, path, sizeof(tmpFd->fileName) - 1);
    else
        snprintf(tmpFd->fileName, sizeof(tmpFd->fileName), "/%s", path);
    tmpFd->fileName[sizeof(tmpFd->fileName) - 1] = '\0';

    /* This Will Set Up The Descriptor Modes */
    tmpFd->mode = 0;
    for (i = 0; '\0' != flags[i]; i++) {
        switch (flags[i]) {
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
                tmpFd->mode |= fileWrite;
                tmpFd->mode |= fileAppend;
                break;
            default:
                kprintf("Invalid mode '%c' for fopen\n", flags[i]);
                break;
        }
    }

    /* Search For The File */
    if (tmpFd->mp->fs->vfsOpenFile(tmpFd->fileName, tmpFd) == 0x1) {
        /* If The File Is Found Then Set Up The Descriptor */

        /* in order to save resources we will allocate the buffer later when it is needed */

        tmpFd->buffer = (char*) kmalloc(4096);

        if (tmpFd->buffer == 0x0) {
            kfree(tmpFd);
            kprintf("Error: tmpFd->buffer == NULL, File: %s, Line: %i\n", __FILE__, __LINE__);
            return (0x0);
        }

        /* Set Its Status To Open */
        tmpFd->status = fdOpen;

        /* Initial File Offset Is Zero */
        tmpFd->offset = 0;
        tmpFd->prev = 0x0;

        /* we do not want to be in a spinlock longer than we need to, so
         it has been moved to here. */
        spinLock(&fdTable_lock);

        /* Increment Number Of Open Files */
        systemVitals->openFiles++;

        tmpFd->next = fdTable;

        if (fdTable != 0x0)
            fdTable->prev = tmpFd;

        fdTable = tmpFd;

        spinUnlock(&fdTable_lock);

        /* Return The FD */
        return (tmpFd);
    }
    else {
        kfree(tmpFd);
        return (0x0);
    }

    /* Return NULL */
    return (0x0);
}

/************************************************************************

 Function: int fclose(fileDescriptor_t *fd);
 Description: This Will Close And Free A File Descriptor
 Notes:

 ************************************************************************/
int fclose(fileDescriptor_t *fd) {
    fileDescriptor_t *tmpFd = 0x0;

    if (fd == 0)
        return (0x0);

    spinLock(&fdTable_lock);

    for (tmpFd = fdTable; tmpFd != 0x0; tmpFd = tmpFd->next) {

        if (tmpFd == fd) {

            if (fd->dup > 0) {
                fd->dup--;
            }
            else {
                if (fd->res != 0x0 && fd->mp != 0x0 &&
                    fd->mp->fs != 0x0 && fd->mp->fs->vfsClose != 0x0)
                    fd->mp->fs->vfsClose(fd->res);

                if (tmpFd->prev)
                    tmpFd->prev->next = tmpFd->next;
                if (tmpFd->next)
                    tmpFd->next->prev = tmpFd->prev;

                if (tmpFd == fdTable)
                    fdTable = tmpFd->next;

                systemVitals->openFiles--;

                spinUnlock(&fdTable_lock);

                if (tmpFd->buffer != NULL) {
                    kfree(tmpFd->buffer);
                }

                kfree(tmpFd);
                return (0x0);
            }
        }
    }

    spinUnlock(&fdTable_lock);
    return (0x1);
}

/* UBU */

/************************************************************************

 Function: void sysMkDir(const char *path)
 Description: This Will Create A New Directory
 Notes:

 ************************************************************************/
/**
 * sysMkDir — Create a directory at `path`.
 *
 * The previous version split the path into parent + basename and tried to
 * fopen() the parent — but the parent is a directory, not a regular file,
 * so fopen() always failed and the error path fired ("invalid mount point
 * for ...") for every interior component on a "mkdir -p /a/b/c" call.
 *
 * The right shape: resolve the mount via vfs_findMount, build a stack-
 * allocated fileDescriptor_t with just the mount pointer set, and hand
 * the full path off to the filesystem's vfsMakeDir.  FAT's mkdir_fat
 * already splits the path and walks parent_cluster internally, so
 * passing the whole path is what it actually expects.
 */
/**
 * sysMkDir — Create a directory at `path`.
 *
 * Returns 0 on success, -EEXIST if the path already names a directory,
 * -EIO on filesystem failure.  The EEXIST signal matters: busybox's
 * mkdir -p walks the path and tolerates EEXIST for each interior
 * level, so without it "mkdir -p /a/b/c" gets -EIO on "/" or "/tmp"
 * and bails out before ever creating the leaves.
 */
int sysMkDir(const char *path) {
    char fullpath[1024];
    fileDescriptor_t mkdir_fd;
    kDIR_t *existing;

    /* Resolve relative paths against CWD. */
    if (path[0] != '/')
        snprintf(fullpath, sizeof(fullpath), "%s%s", _current->oInfo.cwd, path);
    else
        strncpy(fullpath, path, sizeof(fullpath) - 1);
    fullpath[sizeof(fullpath) - 1] = '\0';

    /* busybox mkdir -p passes paths with trailing slashes; fat_dir_mkdir's
     * basename extraction sees "" and the call fails.  Strip them here,
     * but leave a bare "/" alone — mkdir_fat's own basename parser also
     * strips trailing slashes defensively, but doing it here keeps the
     * EEXIST probe below working off the canonical form. */
    {
        size_t n = strlen(fullpath);
        while (n > 1 && fullpath[n - 1] == '/')
            fullpath[--n] = '\0';
    }

    /* If the path already names a directory, report EEXIST and skip the
     * actual create — keeps fat_dir_create_entry from silently writing a
     * duplicate directory entry, and lets mkdir -p walk through existing
     * interior components. */
    existing = vfs_opendir(fullpath);
    if (existing != NULL) {
        vfs_closedir(existing);
        return (-EEXIST);
    }

    struct vfs_mountPoint *mp = vfs_findMount(fullpath);
    if (mp == NULL || mp->fs == NULL) {
        klog(KLOG_ERR, "sysMkDir: no mount for %s", fullpath);
        return (-EIO);
    }
    if (mp->fs->vfsMakeDir == NULL) {
        klog(KLOG_ERR, "sysMkDir: %s: filesystem does not support mkdir", fullpath);
        return (-EIO);
    }

    memset(&mkdir_fd, 0, sizeof(mkdir_fd));
    mkdir_fd.mp = mp;
    if (mp->fs->vfsMakeDir(fullpath, &mkdir_fd) != 0)
        return (-EIO);
    return (0);
}

/************************************************************************

 Function: int unlink(const char *node)
 Description: This will unlink a file
 Notes:

 ************************************************************************/

int unlink(const char *node) {
    char fullpath[1024];
    const char *fs_path = 0x0;
    struct vfs_mountPoint *mp = 0x0;

    if (node == NULL || node[0] == '\0')
        return (0x0);

    if (node[0] != '/')
        snprintf(fullpath, sizeof(fullpath), "%s%s", _current->oInfo.cwd, node);
    else
        strncpy(fullpath, node, sizeof(fullpath) - 1);
    fullpath[sizeof(fullpath) - 1] = '\0';

    mp = vfs_findMount(fullpath);
    if (mp == 0x0) {
        kprintf("DBG: Mount Point Bad");
        return (0x0);
    }

    size_t mlen = strlen(mp->mountPoint);
    fs_path = (mlen > 1) ? fullpath + mlen : fullpath;
    if (fs_path[0] == '\0') fs_path = "/";

    if (mp->fs->vfsUnlink == NULL) {
        klog(KLOG_ERR, "unlink: filesystem does not support unlink for %s", fs_path);
        return (0x0);
    }
    mp->fs->vfsUnlink(fs_path, mp);

    return (0x0);
}

kDIR_t *vfs_opendir(const char *path) {
    char fileName[1024];
    const char *dirPath = 0x0;
    struct vfs_mountPoint *mp = 0x0;
    kDIR_t *dir = 0x0;

    if (path == 0x0 || path[0] == '\0')
        return (0x0);

    /* Resolve relative paths (including '.') against CWD. */
    if (path[0] != '/') {
        snprintf(fileName, sizeof(fileName), "%s%s",
                 _current->oInfo.cwd,
                 (path[0] == '.' && path[1] == '\0') ? "" : path);
    } else {
        strncpy(fileName, path, sizeof(fileName) - 1);
    }
    fileName[sizeof(fileName) - 1] = '\0';

    /* Find mount by longest POSIX prefix match. */
    mp = vfs_findMount(fileName);
    if (mp == 0x0)
        return (0x0);

    /* Strip mount prefix for the FS-relative path. */
    {
        size_t mlen = strlen(mp->mountPoint);
        dirPath = (mlen > 1) ? fileName + mlen : fileName;
        if (dirPath[0] == '\0')
            dirPath = "/";
    }

    if (dirPath[0] != '/') {
        char tmp[1024];
        snprintf(tmp, sizeof(tmp), "/%s", dirPath);
        strncpy(fileName, tmp, sizeof(fileName) - 1);
        fileName[sizeof(fileName) - 1] = '\0';
        dirPath = fileName;
    }

    if (mp == 0x0 || mp->fs->vfsOpenDir == 0x0)
        return (0x0);

    dir = (kDIR_t *) kmalloc(sizeof(kDIR_t));
    if (dir == 0x0)
        return (0x0);

    memset(dir, 0x0, sizeof(kDIR_t));
    dir->mp = mp;

    if (mp->fs->vfsOpenDir(dirPath, dir) != 0x1) {
        kfree(dir);
        return (0x0);
    }

    return (dir);
}

int vfs_readdir(kDIR_t *dir, struct kdirent *ent) {
    if (dir == 0x0 || ent == 0x0 || dir->mp == 0x0 || dir->mp->fs->vfsReadDir == 0x0)
        return (-1);
    return (dir->mp->fs->vfsReadDir(dir, ent));
}

int vfs_closedir(kDIR_t *dir) {
    int ret = 0;
    if (dir == 0x0)
        return (-1);
    if (dir->mp != 0x0 && dir->mp->fs->vfsCloseDir != 0x0)
        ret = dir->mp->fs->vfsCloseDir(dir);
    kfree(dir);
    return (ret);
}

int sys_opendir(struct thread *td, struct sys_opendir_args *args) {
    kDIR_t *kdir = vfs_opendir(args->path);
    args->dir->dd_handle = kdir;
    td->td_retval[0] = (kdir == 0x0) ? -1 : 0;
    return (0);
}

int sys_readdir(struct thread *td, struct sys_readdir_args *args) {
    if (args->dir == 0x0 || args->dir->dd_handle == 0x0) {
        td->td_retval[0] = -1;
        return (0);
    }
    td->td_retval[0] = vfs_readdir(args->dir->dd_handle, &args->dir->dd_ent);
    return (0);
}

int sys_closedir(struct thread *td, struct sys_closedir_args *args) {
    if (args->dir == 0x0 || args->dir->dd_handle == 0x0) {
        td->td_retval[0] = -1;
        return (0);
    }
    td->td_retval[0] = vfs_closedir(args->dir->dd_handle);
    args->dir->dd_handle = 0x0;
    return (0);
}
