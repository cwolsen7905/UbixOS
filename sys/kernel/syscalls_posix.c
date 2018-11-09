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

#include <ubixos/syscalls.h>
#include <sys/sysproto_posix.h>

/* System Calls List */
struct syscall_entry systemCalls_posix[] = {
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 0 */
  {
    ARG_COUNT(sys_exit_args),
    "exit",
    (sys_call_t *) sys_exit,
    SYSCALL_VALID }, /* 1 */
  {
    ARG_COUNT(sys_fork_args),
    "fork",
    (sys_call_t *) sys_fork,
    SYSCALL_VALID }, /* 2 */
  {
    ARG_COUNT(sys_read_args),
    "read",
    (sys_call_t *) sys_read,
    SYSCALL_VALID }, /* 3 */
  {
    ARG_COUNT(sys_write_args),
    "write",
    (sys_call_t *) sys_write,
    SYSCALL_VALID }, /* 4 */
  {
    ARG_COUNT(sys_open_args),
    "open",
    (sys_call_t *) sys_open,
    SYSCALL_VALID }, /* 5 */
  {
    ARG_COUNT(sys_close_args),
    "close",
    (sys_call_t *) sys_close,
    SYSCALL_VALID }, /* 6 */
  {
    ARG_COUNT(sys_wait4_args),
    "wiat4",
    (sys_call_t *) sys_wait4,
    SYSCALL_VALID }, /* 7 */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 8 */
  {
    0,
    "link",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 9 */
  {
    0,
    "unlink",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 10 */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 11 */
  {
    ARG_COUNT(sys_chdir_args),
    "cgdur",
    (sys_call_t *) sys_chdir,
    SYSCALL_VALID }, /* 12 */
  {
    ARG_COUNT(sys_fchdir_args),
    "fchdir",
    sys_fchdir,
    SYSCALL_VALID }, /* 13 */
  {
    0,
    "mknod",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 14 */
  {
    0,
    "chmod",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 15 */
  {
    0,
    "chown",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 16 */
  {
    0,
    "break",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 17 */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 18 */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 19 */
  {
    ARG_COUNT(sys_getpid_args),
    "getpid",
    sys_getpid,
    SYSCALL_VALID }, // 20 - getpid
  {
    0,
    "mount",
    sys_invalid,
    SYSCALL_NOTIMP }, // 21 - mount
  {
    0,
    "unmount",
    sys_invalid,
    SYSCALL_NOTIMP }, // 22 - unmount
  {
    ARG_COUNT(sys_setUID_args),
    "setuid",
    (sys_call_t *) sys_setUID,
    SYSCALL_VALID }, // 23 - setUID
  {
    0,
    "getuid",
    sys_getUID,
    SYSCALL_VALID }, // 24 - getuid
  {
    ARG_COUNT(sys_geteuid_args),
    "geteuid",
    sys_geteuid,
    SYSCALL_VALID }, // 25 - geteuid
  {
    0,
    "ptrace",
    sys_invalid,
    SYSCALL_NOTIMP }, // 26 - ptrace
  {
    0,
    "recvmsg",
    sys_invalid,
    SYSCALL_NOTIMP }, // 27 - recvmsg
  {
    0,
    "sendmsg",
    sys_invalid,
    SYSCALL_NOTIMP }, // 28 - sendmsg
  {
    0,
    "recvfrom",
    sys_invalid,
    SYSCALL_NOTIMP }, // 29 - recvfrom
  {
    0,
    "accept",
    sys_invalid,
    SYSCALL_NOTIMP }, // 30 - accept
  {
    0,
    "getpeername",
    sys_invalid,
    SYSCALL_NOTIMP }, // 31 - getpeername
  {
    0,
    "getsockname",
    sys_invalid,
    SYSCALL_NOTIMP }, // 32 - getsockname
  {
    ARG_COUNT(sys_access_args),
    "access",
    sys_access,
    SYSCALL_VALID }, /* 33 */
  {
    0,
    "chflags",
    sys_invalid,
    SYSCALL_NOTIMP }, // 34 - chflags
  {
    0,
    "fchflags",
    sys_invalid,
    SYSCALL_NOTIMP }, // 35 - fchflags
  {
    0,
    "sync",
    sys_invalid,
    SYSCALL_NOTIMP }, // 36 - sync
  {
    0,
    "kill",
    sys_invalid,
    SYSCALL_NOTIMP }, // 37 - kill
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, // 38
  {
    ARG_COUNT(sys_getppid_args),
    "getpid",
    sys_getpid,
    SYSCALL_VALID }, // 39 - getppid
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, // 40
  {
    0,
    "dup",
    sys_invalid,
    SYSCALL_NOTIMP }, // 41 - dup
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, // 42
  {
    ARG_COUNT(sys_getegid_args),
    "getegid",
    sys_getegid,
    SYSCALL_VALID }, // 43 - getegid
  {
    0,
    "profil",
    sys_invalid,
    SYSCALL_NOTIMP }, // 44  - profil
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_NOTIMP }, // 45 - ktrace
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, // 46
  {
    0,
    "getuid",
    sys_getGID,
    SYSCALL_VALID }, // 47 - getgid
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, // 48
  {
    0,
    "getlogin",
    sys_invalid,
    SYSCALL_NOTIMP }, // 49 - getlogin
  {
    0,
    "setlogin",
    sys_invalid,
    SYSCALL_NOTIMP }, // 50 - setlogin
  {
    0,
    "acct",
    sys_invalid,
    SYSCALL_NOTIMP }, // 51 - acct
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, // 52
  {
    0,
    "sigaltstack",
    sys_invalid,
    SYSCALL_NOTIMP }, // 53 - sigaltstack
  {
    ARG_COUNT(sys_ioctl_args),
    "ioctl",
    sys_ioctl,
    SYSCALL_VALID }, // 54 - ioctl
  {
    0,
    "reboot",
    sys_invalid,
    SYSCALL_NOTIMP }, // 55 - reboot
  {
    0,
    "revoke",
    sys_invalid,
    SYSCALL_NOTIMP }, // 56 - revoke
  {
    0,
    "symlink",
    sys_invalid,
    SYSCALL_NOTIMP }, // 57 - symlink
  {
    0,
    "readlink",
    sys_invalid,
    SYSCALL_NOTIMPD }, // 58 - readlink
  {
    ARG_COUNT(sys_execve_args),
    "execve",
    (sys_call_t *) sys_execve,
    SYSCALL_VALID }, // 59 - execv
  {
    0,
    "umask",
    sys_invalid,
    SYSCALL_NOTIMP }, //  60 - umask
  {
    0,
    "chroot",
    sys_invalid,
    SYSCALL_NOTIMP }, //  61 - chroot
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, //  62
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, //  63
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, //  64
  {
    0,
    "msync",
    sys_invalid,
    SYSCALL_NOTIMP }, //  65 - msync
  {
    0,
    "vfork",
    sys_invalid,
    SYSCALL_NOTIMP }, //  66 - vfork
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /*  67 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /*  68 - Invalid */
  {
    0,
    "sbrk",
    sys_invalid,
    SYSCALL_NOTIMP }, /*  69 - sbrk */
  {
    0,
    "sstk",
    sys_invalid,
    SYSCALL_NOTIMP }, /*  70 - sstk */
  {
    0,
    "old mmap",
    sys_invalid,
    SYSCALL_INVALID }, //  71
  {
    0,
    "vadvise",
    sys_invalid,
    SYSCALL_NOTIMP }, /*  72 - vadvise */
  {
    ARG_COUNT(sys_munmap_args),
    "munmap",
    sys_munmap,
    SYSCALL_VALID }, //  73 - munmap
  {
    0,
    "mprotect",
    sys_invalid,
    SYSCALL_NOTIMP }, //  74 - mprotect
  {
    0,
    "madvise",
    sys_invalid,
    SYSCALL_NOTIMP }, //  75 - madvise
  {
    0,
    "Obsolete vhangup",
    sys_invalid,
    SYSCALL_INVALID }, /*  76 - Invalid */
  {
    0,
    "Obsolete vlimit",
    sys_invalid,
    SYSCALL_INVALID }, /*  77 - Invalid */
  {
    0,
    "mincore",
    sys_invalid,
    SYSCALL_NOTIMP }, /*  78 - minicore */
  {
    0,
    "getgroups",
    sys_invalid,
    SYSCALL_NOTIMP }, /*  79 - getgroups */
  {
    0,
    "setgroups",
    sys_invalid,
    SYSCALL_NOTIMP }, /*  80 - setgroups */
  {
    ARG_COUNT(sys_getpgrp_args),
    "getpgrp",
    sys_getpgrp,
    SYSCALL_VALID }, //  81 - getpgrp
  {
    ARG_COUNT(sys_setpgid_args),
    "setpgid",
    sys_setpgid,
    SYSCALL_VALID }, //  82 - setpgid
  {
    0,
    "setitimer",
    sys_invalid,
    SYSCALL_NOTIMP }, /*  83 - setitimer */
  {
    0,
    "old wait",
    sys_invalid,
    SYSCALL_INVALID }, /*  84 - Invalid */
  {
    0,
    "swapon",
    sys_invalid,
    SYSCALL_NOTIMP }, /*  85 - swapon */
  {
    0,
    "getitimer",
    sys_invalid,
    SYSCALL_NOTIMP }, /*  86 - getitimer */
  {
    0,
    "old gethostname",
    sys_invalid,
    SYSCALL_INVALID }, /*  87  */
  {
    0,
    "Get Free Page",
    (sys_call_t *) sysGetFreePage,
    SYSCALL_VALID }, /*  88 - getFreePage TEMP OLD sethostname */
  {
    0,
    "getdtablesize",
    sys_invalid,
    SYSCALL_NOTIMP }, /*  89 - gettablesize */
  {
    0,
    "dup2",
    sys_invalid,
    SYSCALL_NOTIMP }, /*  90 - dup2 */
  {
    0,
    "getdopt",
    sys_invalid,
    SYSCALL_NOTIMP }, /*  91 - getdopt */
  {
    ARG_COUNT(sys_fcntl_args),
    "fcntl",
    sys_fcntl,
    SYSCALL_VALID }, //  92 - fcntl
  {
    ARG_COUNT(sys_select_args),
    "select",
    sys_select,
    SYSCALL_VALID }, // 93 - select
  {
    0,
    "setdopt",
    sys_invalid,
    SYSCALL_NOTIMP }, /*  94 - setdopt */
  {
    0,
    "fsync",
    sys_invalid,
    SYSCALL_NOTIMP }, /*  95 - fsync */
  {
    0,
    "setpriority",
    sys_invalid,
    SYSCALL_NOTIMP }, /*  96 - setpriority */
  {
    ARG_COUNT(sys_socket_args),
    "socket",
    sys_socket,
    SYSCALL_VALID }, //  97 - socket
  {
    0,
    "connect",
    sys_invalid,
    SYSCALL_NOTIMP }, /*  98 - connect */
  {
    0,
    "old accept",
    sys_invalid,
    SYSCALL_INVALID }, /*  99  */
  {
    0,
    "getpriority",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 100 - getpriority */
  {
    0,
    "old send",
    sys_invalid,
    SYSCALL_INVALID }, /* 101 */
  {
    0,
    "old recv",
    sys_invalid,
    SYSCALL_INVALID }, /* 102 */
  {
    0,
    "old sigreturn",
    sys_invalid,
    SYSCALL_INVALID }, /* 103 */
  {
    0,
    "bind",
    sys_invalid,
    SYSCALL_NOTIMP }, // 104 - bind
  {
    ARG_COUNT(sys_setsockopt_args),
    "setsockopt",
    sys_setsockopt,
    SYSCALL_VALID }, // 105 setsockopt
  {
    0,
    "listen",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 106 - listen */
  {
    0,
    "obsolete vtimes",
    sys_invalid,
    SYSCALL_INVALID }, /* 107  */
  {
    0,
    "old sigvec",
    sys_invalid,
    SYSCALL_INVALID }, /* 108 - Invalid */
  {
    0,
    "old sigblock",
    sys_invalid,
    SYSCALL_INVALID }, /* 109 - Invalid */
  {
    0,
    "old sigsetmask",
    sys_invalid,
    SYSCALL_INVALID }, /* 110 - Invalid */
  {
    0,
    "old sigsuspend",
    sys_invalid,
    SYSCALL_INVALID }, /* 111 - Invalid */
  {
    0,
    "old sigstack",
    sys_invalid,
    SYSCALL_INVALID }, /* 112 - Invalid */
  {
    0,
    "old recvmsg",
    sys_invalid,
    SYSCALL_INVALID }, /* 113 - Invalid */
  {
    0,
    "old sendmsg",
    sys_invalid,
    SYSCALL_INVALID }, /* 114 - Invalid */
  {
    0,
    "obsolete vtrace",
    sys_invalid,
    SYSCALL_INVALID }, /* 115 - Invalid */
  {
    ARG_COUNT(sys_gettimeofday_args),
    "gettimeofday",
    sys_gettimeofday,
    SYSCALL_VALID }, // 116 - gettimeofday
  {
    0,
    "getrusage",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 117 - getrusage */
  {
    0,
    "getsockopt",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 118 - getsockopt */
  {
    0,
    "resuba",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 119 - resuba */
  {
    0,
    "readv",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 120 - readv */
  {
    0,
    "writev",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 121 - writev */
  {
    0,
    "settimeofday",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 122 - settimeofday */
  {
    0,
    "fchown",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 123 - fchown */
  {
    0,
    "fchmod",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 124 - fchmod */
  {
    0,
    "old recvfrom",
    sys_invalid,
    SYSCALL_INVALID }, /* 125 - Invalid */
  {
    0,
    "setreuid",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 126 - setreuid */
  {
    0,
    "setregid",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 127 - setregid */
  {
    ARG_COUNT(sys_rename_args),
    "rename",
    sys_rename,
    SYSCALL_VALID }, /* 128 - rename */
  {
    0,
    "old truncate",
    sys_invalid,
    SYSCALL_INVALID }, /* 129 - Invalid */
  {
    0,
    "old fruncate",
    sys_invalid,
    SYSCALL_INVALID }, /* 130 - Invalid */
  {
    0,
    "flock",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 131 - flock */
  {
    0,
    "mkfifo",
    sys_invalid,
    SYSCALL_NOTMP }, /* 132 - mkfifo */
  {
    ARG_COUNT(sys_sendto_args),
    "sendto",
    sys_sendto,
    SYSCALL_VALID }, // 133 - sendto
  {
    0,
    "shutdown",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 134 - shutdown */
  {
    0,
    "socketpair",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 135 - socketpair */
  {
    0,
    "mkdir",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 136 - mkdir */
  {
    0,
    "rmdir",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 137 - rmdir */
  {
    0,
    "utimes",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 138 - utimes */
  {
    0,
    "obsolete sigreturn",
    sys_invalid,
    SYSCALL_INVALID }, /* 139 - Invalid */
  {
    0,
    "adjtime",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 140 - adjtime */
  {
    0,
    "old getpeername",
    sys_invalid,
    SYSCALL_INVALID }, /* 141 - Invalid */
  {
    0,
    "ikd gethostid",
    sys_invalid,
    SYSCALL_INVALID }, /* 142 - Invalid */
  {
    0,
    "old sethostid",
    sys_invalid,
    SYSCALL_INVALID }, /* 143 - Invalid */
  {
    0,
    "old getrlimit",
    sys_invalid,
    SYSCALL_INVALID }, /* 144 - Invalid */
  {
    0,
    "old setrlimit",
    sys_invalid,
    SYSCALL_INVALID }, /* 145 - Invalid */
  {
    0,
    "old killpg",
    sys_invalid,
    SYSCALL_INVALID }, /* 146 - Invalid */
  {
    0,
    "setsid",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 147 - setsid */
  {
    0,
    "quotactl",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 148 - quotactl */
  {
    0,
    "old quota",
    sys_invalid,
    SYSCALL_INVALID }, /* 149 - Invalid */
  {
    0,
    "old getsockname",
    sys_invalid,
    SYSCALL_INVALID }, /* 150 - Invalid */
  {
    0,
    "sem_lock",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 151 - sem_lock */
  {
    0,
    "sem_wakeup",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 152 - sem_Wakeup */
  {
    0,
    "asyncdaemon",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 153 - asyncdaemon */
  {
    0,
    "nlm_syscall",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 154 - nlm_syscall */
  {
    0,
    "nfssvc",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 155 - nfssvc */
  {
    0,
    "old getdirentries",
    sys_invalid,
    SYSCALL_INVALID }, /* 156 - Invalid */
  {
    0,
    "old statfs",
    sys_invalid,
    SYSCALL_INVALID }, /* 157 - Invalid */
  {
    0,
    "old fstatfs",
    sys_invalid,
    SYSCALL_INVALID }, /* 158 - Invalid */
  {
    0,
    "nosys",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 159 - nosys */
  {
    0,
    "lgetfh",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 160 - lgetfh */
  {
    0,
    "getfh",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 161 - Invalid */
  {
    0,
    "old getdomainname",
    sys_invalid,
    SYSCALL_INVALID }, /* 162 - Invalid */
  {
    0,
    "old setdomainname",
    sys_invalid,
    SYSCALL_INVALID }, /* 163 - Invalid */
  {
    0,
    "old uname",
    sys_invalid,
    SYSCALL_INVALID }, /* 164 - Invalid */
  {
    ARG_COUNT(sys_sysarch_args),
    "sysarch",
    sys_sysarch,
    SYSCALL_VALID }, // 165 - sysarch
  {
    0,
    "rtprio",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 166 - rtprio */
  {
    0,
    "nosys",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 167 - nosys */
  {
    0,
    "nosys",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 168 - nosys */
  {
    0,
    "semsys",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 169 - semsys */
  {
    0,
    "msgsys",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 170 - msgsys */
  {
    0,
    "shmsys",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 171 - shmsys */
  {
    0,
    "nosys",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 172 - nosys */
  {
    0,
    "old pread",
    sys_invalid,
    SYSCALL_INVALID }, /* 173 - Invalid */
  {
    0,
    "old pwrite",
    sys_invalid,
    SYSCALL_INVALID }, /* 174 - Invalid */
  {
    0,
    "setfib",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 175 - setfib */
  {
    0,
    "ntp_adjtime",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 176 - ntp_adjtime */
  {
    0,
    "sfork",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 177 - sfork */
  {
    0,
    "getdescriptor",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 178 - getdescriptor */
  {
    0,
    "setdescriptor",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 179 - setdescriptor */
  {
    0,
    "nosys",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 180 - Invalid */
  {
    ARG_COUNT(sys_setGID_args),
    "setgid",
    (sys_call_t *) sys_setGID,
    SYSCALL_VALID }, /* 181 - setgid */
  {
    0,
    "setegid",
    sys_invalid,
    SYSCALL_NOTIMP }, // 182 - setegid
  {
    0,
    "seteuid",
    sys_invalid,
    SYSCALL_NOTIMP }, // 183 - seteuid
  {
    0,
    "lfs_bmapv",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 184 - Invalid */
  {
    0,
    "lfs_markv",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 185 - Invalid */
  {
    0,
    "lfs_segclean",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 186 - Invalid */
  {
    0,
    "lfs_segwait",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 187 - Invalid */
  {
    ARG_COUNT(sys_stat_args),
    "stat",
    (sys_call_t *) sys_stat,
    SYSCALL_VALID }, /* 188 - sys_stat */
  {
    ARG_COUNT(sys_fstat_args),
    "fstat",
    (sys_call_t *) sys_fstat,
    SYSCALL_VALID }, /* 189 - sys_fstat */
  {
    ARG_COUNT(sys_lstat_args),
    "lstat",
    (sys_call_t *) sys_lstat,
    SYSCALL_VALID }, /* 190 - sys_lstat */
  {
    0,
    "pathconf",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 191 - Invalid */
  {
    0,
    "fpathconf",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 192 - Invalid */
  {
    0,
    "nosys",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 193 - Invalid */
  {
    0,
    "getrlimit",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 194 - Invalid */
  {
    0,
    "setrlimit",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 195 - Invalid */
  {
    ARG_COUNT(sys_getdirentries_args),
    "getdirentries",
    sys_getdirentries,
    SYSCALL_VALID }, // 196 - getdirentries
  {
    ARG_COUNT(sys_mmap_args),
    "old mmap",
    (sys_call_t *) sys_mmap,
    SYSCALL_INVALID }, /* 197 - sys_mmap */
  {
    0,
    "__syscall",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 198 - Invalid */
  {
    0,
    "old lseek",
    sys_invalid,
    SYSCALL_INVALID }, /* 199 - Invalid */
  {
    0,
    "old truncate",
    sys_invalid,
    SYSCALL_INVALID }, /* 200 - Invalid */
  {
    0,
    "old fruncate",
    sys_invalid,
    SYSCALL_INVALID }, /* 201 - Invalid */
  {
    ARG_COUNT(sys_sysctl_args),
    "__sysctl",
    (sys_call_t *) sys_sysctl,
    SYSCALL_VALID }, /* 202 - sys_sysctl */
  {
    0,
    "mlock",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 203 - Invalid */
  {
    0,
    "munlock",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 204 - Invalid */
  {
    0,
    "undelete",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 205 - Invalid */
  {
    0,
    "futimes",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 206 - Invalid */
  {
    0,
    "getpgid",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 207 - Invalid */
  {
    0,
    "reboot",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 208 - Invalid */
  {
    0,
    "poll",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 209 - Invalid */
  {
    0,
    "lkmnosys",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 210 - Invalid */
  {
    0,
    "lkmnosys",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 211 - Invalid */
  {
    0,
    "lkmnosys",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 212 - Invalid */
  {
    0,
    "lkmnosys",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 213 - Invalid */
  {
    0,
    "lkmnosys",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 214 - Invalid */
  {
    0,
    "lkmnosys",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 215 - Invalid */
  {
    0,
    "lkmnosys",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 216 - Invalid */
  {
    0,
    "lkmnosys",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 217 - Invalid */
  {
    0,
    "lkmnosys",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 218 - Invalid */
  {
    0,
    "lkmnosys",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 219 - Invalid */
  {
    0,
    "old __semctl",
    sys_invalid,
    SYSCALL_INVALID }, /* 220 - Invalid */
  {
    0,
    "semget",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 221 - Invalid */
  {
    0,
    "semop",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 222 - Invalid */
  {
    0,
    "semconfig",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 223 - Invalid */
  {
    0,
    "old msgctl",
    sys_invalid,
    SYSCALL_INVALID }, /* 224 - Invalid */
  {
    0,
    "msgget",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 225 - Invalid */
  {
    0,
    "msgsnd",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 226 - Invalid */
  {
    0,
    "msgrcv",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 227 - Invalid */
  {
    0,
    "shmat",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 228 - Invalid */
  {
    0,
    "old shmctl",
    sys_invalid,
    SYSCALL_INVALID }, /* 229 - Invalid */
  {
    0,
    "shmdt",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 230 - Invalid */
  {
    0,
    "shmget",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 231 - Invalid */
  {
    0,
    "clock_gettime",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 232 - Invalid */
  {
    0,
    "clock_settime",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 233 - Invalid */
  {
    0,
    "clock_getres",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 234 - Invalid */
  {
    0,
    "ktimer_create",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 235 - Invalid */
  {
    0,
    "ktimer_delete",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 236 - Invalid */
  {
    0,
    "ktimer_settime",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 237 - Invalid */
  {
    0,
    "ktimer_gettime",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 238 - Invalid */
  {
    0,
    "ktimer_getoverrun",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 239 - Invalid */
  {
    0,
    "nanosleep",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 240 - Invalid */
  {
    0,
    "ffclock_getcounter",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 241 - Invalid */
  {
    0,
    "ffclock_setestimate",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 242 - Invalid */
  {
    0,
    "fflock_getestimate",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 243 - Invalid */
  {
    0,
    "clock_nanosleep",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 244 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 245 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 246 - Invalid */
  {
    0,
    "clock_getcpuclockid2",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 247 - Invalid */
  {
    0,
    "ntp_gettime",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 248 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 249 - Invalid */
  {
    0,
    "minherit",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 250 - Invalid */
  {
    0,
    "rfork",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 251 - Invalid */
  {
    0,
    "openbsd_poll",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 252 - Invalid */
  {
    ARG_COUNT(sys_issetugid_args),
    "issetugid",
    (sys_call_t *) sys_issetugid,
    SYSCALL_VALID }, /* 253 - Invalid */
  {
    0,
    "lchown",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 254 - Invalid */
  {
    0,
    "aio_read",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 255 - Invalid */
  {
    0,
    "aio_write",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 256 - Invalid */
  {
    0,
    "lio_listio",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 257 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 258 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 259 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 260 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 261 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 262 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 263 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 264 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 265 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 266 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 267 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 268 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 269 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 270 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 271 - Invalid */
  {
    0,
    "getdents",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 272 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 273 - Invalid */
  {
    0,
    "lchmod",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 274 - Invalid */
  {
    0,
    "netbsd_lchown",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 275 - Invalid */
  {
    0,
    "lutimes",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 276 - Invalid */
  {
    0,
    "netbsd_msync",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 277 - Invalid */
  {
    0,
    "nstat",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 278 - Invalid */
  {
    0,
    "nfstat",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 279 - Invalid */
  {
    0,
    "nlstat",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 280 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 281 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 282 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 283 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 284 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 285 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 286 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 287 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 288 - Invalid */
  {
    0,
    "preadv",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 289 - Invalid */
  {
    0,
    "pwritev",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 290 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 291 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 292 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_INVALID }, /* 293 - Invalid */
  {
    ARG_COUNT(sys_fseek_args),
    "feek",
    (sys_call_t *) sys_fseek,
    SYSCALL_INVALID }, /* 294 - fseek */
  {
    ARG_COUNT(sys_fgetc_args),
    "fgetc",
    (sys_call_t *) sys_fgetc,
    SYSCALL_INVALID }, /* 295 - fgetc */
  {
    ARG_COUNT(sys_fclose_args),
    "flose",
    (sys_call_t *) sys_fclose,
    SYSCALL_INVALID }, /* 296 - fclose */
  {
    ARG_COUNT(sys_fread_args),
    "fread",
    (sys_call_t *) sys_fread,
    SYSCALL_INVALID }, /* 297 - fread */
  {
    ARG_COUNT(sys_fopen_args),
    "fopen",
    (sys_call_t *) sys_fopen,
    SYSCALL_INVALID }, /* 298 - fopen */
  {
    0,
    "fhstat",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 299 - Invalid */
  {
    0,
    "modnext",
    sys_invalid,
    SYSCALL_NOTIMP }, /* 300 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 301 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 302 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 303 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 304 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 305 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 306 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 307 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 308 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 309 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 310 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 311 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 312 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 313 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 314 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 315 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 316 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 317 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 318 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 319 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 320 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 321 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 322 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 323 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 324 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 325 - Invalid */
  {
    ARG_COUNT(sys_getcwd_args),
    "Get CWD",
    (sys_call_t *) sys_getcwd,
    SYSCALL_VALID }, /* 326 - sys_getcwd */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 327 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 328 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 329 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 330 - Invalid */
  {
    0,
    "Sched Yield",
    sys_sched_yield,
    SYSCALL_VALID }, /* 331 - sys_sched_yield */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 332 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 333 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 334 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 335 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 336 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 337 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 338 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 339 - Invalid */
  {
    ARG_COUNT(sys_sigprocmask_args),
    "sigprocmask",
    sys_sigprocmask,
    SYSCALL_VALID }, // 340 - sigprocmask
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 341 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 342 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 343 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 344 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 345 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 346 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 347 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 348 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 349 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 350 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 351 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 352 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 353 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 354 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 355 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 356 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 357 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 358 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 359 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 350 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 351 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 352 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 353 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 354 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 355 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 356 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 357 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 358 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 359 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 360 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 361 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 362 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 363 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 364 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 365 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 366 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 367 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 368 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 369 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 370 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 371 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 372 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 373 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 374 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 375 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 376 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 377 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 378 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 379 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 380 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 381 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 382 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 383 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 384 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 385 - Invalid */
  {
    ARG_COUNT(sys_statfs_args),
    "statfs",
    (sys_call_t *) sys_statfs,
    SYSCALL_VALID }, // 396 statfs
  {
    ARG_COUNT(sys_fstatfs_args),
    "fstatfs",
    (sys_call_t *) sys_fstatfs,
    SYSCALL_VALID }, // 397 fstatfs
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 398 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 399 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 400 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 401 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 402 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 403 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 404 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 405 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 306 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 307 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 308 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 409 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 410 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 411 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 412 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 413 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 414 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 415 - Invalid */
  {
    ARG_COUNT(sys_sigaction_args),
    "sigaction",
    sys_sigaction,
    SYSCALL_VALID }, // 416 - sigaction
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 417 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 418 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 419 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 410 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 421 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 422 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 423 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 424 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 425 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 426 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 427 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 428 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 429 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 430 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 431 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 432 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 433 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 434 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 435 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 436 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 437 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 438 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 439 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 440 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 441 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 442 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 443 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 444 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 445 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 446 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 447 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 448 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 449 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 450 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 451 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 452 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 453 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 454 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 455 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 456 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 457 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 458 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 459 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 460 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 461 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 462 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 463 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 464 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 465 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 466 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 467 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 468 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 469 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 470 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 471 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 472 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 473 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 474 - Invalid */
  {
    ARG_COUNT(sys_pread_args),
    "pread",
    sys_pread,
    SYSCALL_VALID }, // 475 - pread
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 476 - Invalid */
  {
    ARG_COUNT(sys_mmap_args),
    "MMAP",
    (sys_call_t *) sys_mmap,
    SYSCALL_VALID }, /* 477 - sys_mmap */
  {
    ARG_COUNT(sys_lseek_args),
    "lseek",
    (sys_call_t *) sys_lseek,
    SYSCALL_VALID }, /* 478 - sys_lseek */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 359 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 350 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 351 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 352 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 353 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 354 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 355 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 356 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 357 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 358 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 359 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 350 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 351 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 352 - Invalid */
  {
    ARG_COUNT(sys_fstatat_args),
    "fstatat",
    sys_fstatat,
    SYSCALL_VALID }, // 493 - fstatat
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 354 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 355 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 356 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 357 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 358 - Invalid */
  {
    ARG_COUNT(sys_openat_args),
    "SYS_openat",
    sys_openat,
    SYSCALL_VALID }, /* 499 - sys_openat */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 350 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 351 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 352 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 353 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 354 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 355 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 356 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 357 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 358 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 359 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 360 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 361 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 362 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 363 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 364 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 365 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 366 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 367 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 368 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 369 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 370 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 371 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 372 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 373 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 374 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 375 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 376 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 377 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 378 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 379 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 380 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 381 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 382 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 383 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 384 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 385 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 386 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 387 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 388 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 389 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 390 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 391 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 392 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 393 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 394 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 395 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 396 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 397 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 398 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 399 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 400 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 401 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 402 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 403 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 404 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 405 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 406 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 407 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 408 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 409 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 410 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 411 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 412 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 413 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 414 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 415 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 416 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 417 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 418 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 419 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 420 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 421 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 422 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 423 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 424 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 425 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 426 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 427 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 428 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 429 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 430 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 431 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 432 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 433 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 434 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 435 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 436 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 437 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 438 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 439 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 440 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 441 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 442 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 443 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 444 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 445 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 446 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 447 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 448 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 449 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 450 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 451 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 452 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 453 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 454 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 455 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 456 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 457 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 458 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 459 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 460 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 461 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 462 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 463 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 464 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 465 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 466 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 467 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 468 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 469 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 470 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 471 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 472 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 473 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 474 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 475 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 476 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 477 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 478 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 479 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 480 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 481 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 482 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 483 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 484 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 485 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 486 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 487 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 488 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 489 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 490 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 491 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 492 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 493 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 494 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 495 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 496 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 497 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 498 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 499 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 500 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 501 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 502 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 503 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 504 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 505 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 506 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 507 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 508 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 509 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 510 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 511 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 512 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 513 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 514 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 515 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 516 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 517 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 518 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 519 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 520 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 521 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 522 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 523 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 524 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 525 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 526 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 527 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 528 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 529 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 530 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 531 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 532 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 533 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 534 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 535 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 536 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 537 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 538 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 539 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 540 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 541 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 542 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 543 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 544 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 545 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 546 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 547 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 548 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 549 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 550 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 551 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 552 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 553 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 554 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 555 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 556 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 557 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 558 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 559 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 560 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 561 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 562 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 563 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 564 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 565 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 566 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 567 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 568 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 569 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 570 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 571 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 572 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 573 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 574 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 575 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 576 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 577 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 578 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 579 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 580 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 581 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 582 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 583 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 584 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 585 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 586 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 587 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 588 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 589 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 590 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 591 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 592 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 593 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 594 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 595 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 596 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 597 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 598 - Invalid */
  {
    0,
    "No Call",
    sys_invalid,
    SYSCALL_VALID }, /* 599 - Invalid */
};

int totalCalls_posix = sizeof(systemCalls_posix) / sizeof(struct syscall_entry);
