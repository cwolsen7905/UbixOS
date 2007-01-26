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

#ifndef _SYSPROTO_H
#define _SYSPROTO_H

#include <sys/signal.h>
#include <sys/thread.h>

typedef int register_t;

#define PAD_(t) (sizeof(register_t) <= sizeof(t) ? \
                0 : sizeof(register_t) - sizeof(t))

#if BYTE_ORDER == LITTLE_ENDIAN
#define PADL_(t)        0
#define PADR_(t)        PAD_(t)
#else
#define PADL_(t)        PAD_(t)
#define PADR_(t)        0
#endif

//Protos
struct fork_args {
  register_t dummy;
  };
struct read_args {
  char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
  char buf_l_[PADL_(void *)]; void * buf; char buf_r_[PADR_(void *)];
  char nbyte_l_[PADL_(size_t)]; size_t nbyte; char nbyte_r_[PADR_(size_t)];
  };
struct write_args {
  char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
  char buf_l_[PADL_(const void *)]; const void * buf; char buf_r_[PADR_(const void *)];
  char nbyte_l_[PADL_(size_t)]; size_t nbyte; char nbyte_r_[PADR_(size_t)];
  };
struct open_args {
  char path_l_[PADL_(char *)]; char * path; char path_r_[PADR_(char *)];
  char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
  char mode_l_[PADL_(int)]; int mode; char mode_r_[PADR_(int)];
  };
struct close_args {
  char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
  };

struct setitimer_args {
  char which_l_[PADL_(u_int)]; u_int which; char which_r_[PADR_(u_int)];
  char itv_l_[PADL_(struct itimerval *)]; struct itimerval * itv; char itv_r_[PADR_(struct itimerval *)];
  char oitv_l_[PADL_(struct itimerval *)]; struct itimerval * oitv; char oitv_r_[PADR_(struct itimerval *)];
  };

struct access_args {
  char path_l_[PADL_(char *)]; char * path; char path_r_[PADR_(char *)];
  char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
  };
struct fstatfs_args {
  char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
  char buf_l_[PADL_(struct statfs *)]; struct statfs * buf; char buf_r_[PADR_(struct statfs *)];
  };
struct mprotect_args {
  char addr_l_[PADL_(const void *)]; const void * addr; char addr_r_[PADR_(const void *)];
  char len_l_[PADL_(size_t)]; size_t len; char len_r_[PADR_(size_t)];
  char prot_l_[PADL_(int)]; int prot; char prot_r_[PADR_(int)];
  };
struct lseek_args {
  char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
  char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
  char offset_l_[PADL_(off_t)]; off_t offset; char offset_r_[PADR_(off_t)];
  char whence_l_[PADL_(int)]; int whence; char whence_r_[PADR_(int)];
  };




//Old

struct sysctl_args {
        char name_l_[PADL_(int *)]; int * name; char name_r_[PADR_(int *)];
        char namelen_l_[PADL_(u_int)]; u_int namelen; char namelen_r_[PADR_(u_int)];
        char old_l_[PADL_(void *)]; void * old; char old_r_[PADR_(void *)];
        char oldlenp_l_[PADL_(size_t *)]; size_t * oldlenp; char oldlenp_r_[PADR_(size_t *)];
        char new_l_[PADL_(void *)]; void * new; char new_r_[PADR_(void *)];
        char newlen_l_[PADL_(size_t)]; size_t newlen; char newlen_r_[PADR_(size_t)];
};

struct getpid_args {
        register_t dummy;
};
struct issetugid_args {
        register_t dummy;
};
struct fcntl_args {
        char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
        char cmd_l_[PADL_(int)]; int cmd; char cmd_r_[PADR_(int)];
        char arg_l_[PADL_(long)]; long arg; char arg_r_[PADR_(long)];
};

struct pipe_args {
  register_t dummy;
  };

struct readlink_args {
        char path_l_[PADL_(char *)]; char * path; char path_r_[PADR_(char *)];
        char buf_l_[PADL_(char *)]; char * buf; char buf_r_[PADR_(char *)];
        char count_l_[PADL_(int)]; int count; char count_r_[PADR_(int)];
};

struct getuid_args {
        register_t dummy;
};

struct getgid_args {
        register_t dummy;
};

struct mmap_args {
   char addr_l_[PADL_(caddr_t)]; caddr_t addr; char addr_r_[PADR_(caddr_t)];
  char len_l_[PADL_(size_t)]; size_t len; char len_r_[PADR_(size_t)];
  char prot_l_[PADL_(int)]; int prot; char prot_r_[PADR_(int)];
  char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
  char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
  char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
  char pos_l_[PADL_(off_t)]; off_t pos; char pos_r_[PADR_(off_t)];
  };

struct obreak_args {
  char nsize_l_[PADL_(char *)];char * nsize;char nsize_r_[PADR_(char *)];
  };

struct sigaction_args {
  char sig_l_[PADL_(int)]; int sig; char sig_r_[PADR_(int)];
  char act_l_[PADL_(const struct sigaction *)]; const struct sigaction * act; char act_r_[PADR_(const struct sigaction *)];
  char oact_l_[PADL_(struct sigaction *)]; struct sigaction * oact; char oact_r_[PADR_(struct sigaction *)];
  };

struct getdtablesize_args {
        register_t dummy;
};

struct munmap_args {
        char addr_l_[PADL_(void *)]; void * addr; char addr_r_[PADR_(void *)];
        char len_l_[PADL_(size_t)]; size_t len; char len_r_[PADR_(size_t)];
};

struct sigprocmask_args {
  char how_l_[PADL_(int)]; int how; char how_r_[PADR_(int)];
  char set_l_[PADL_(const sigset_t *)]; const sigset_t * set; char set_r_[PADR_(const sigset_t *)];
  char oset_l_[PADL_(sigset_t *)]; sigset_t * oset; char oset_r_[PADR_(sigset_t *)];
  };
struct gettimeofday_args {
  char tp_l_[PADL_(struct timeval *)]; struct timeval * tp; char tp_r_[PADR_(struct timeval *)];
  char tzp_l_[PADL_(struct timezone *)]; struct timezone * tzp; char tzp_r_[PADR_(struct timezone *)];
  };
struct fstat_args {
        char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
        char sb_l_[PADL_(struct stat *)]; struct stat * sb; char sb_r_[PADR_(struct stat *)];
};
struct ioctl_args {
        char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
        char com_l_[PADL_(u_long)]; u_long com; char com_r_[PADR_(u_long)];
        char data_l_[PADL_(caddr_t)]; caddr_t data; char data_r_[PADR_(caddr_t)];
};

//Func Defs
int fork(struct thread *td,struct fork_args *uap);
int read(struct thread *td,struct read_args *uap);
int write(struct thread *td, struct write_args *uap);
int open(struct thread *td, struct open_args *uap);
int close(struct thread *td, struct close_args *uap);
int setitimer(struct thread *td, struct setitimer_args *uap);
int access(struct thread *td, struct access_args *uap);
int fstatfs(struct thread *td, struct fstatfs_args *uap);
int mprotect(struct thread *td, struct mprotect_args *uap);
int lseek(struct thread *td, struct lseek_args *uap);

#endif

/***
 END
 ***/

