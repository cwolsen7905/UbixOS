# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../../../Makefile.inc
include ../../../Makefile.inc
include ../Makefile.inc

INCLUDES += -I../

CFLAGS += -fpic -DPIC

#Objects
OBJS = __acl_aclcheck_fd.o __acl_aclcheck_file.o __acl_aclcheck_link.o __acl_delete_fd.o __acl_delete_file.o __acl_delete_link.o __acl_get_fd.o __acl_get_file.o __acl_get_link.o __acl_set_fd.o __acl_set_file.o __acl_set_link.o __getcwd.o __mac_execve.o __mac_get_fd.o __mac_get_file.o __mac_get_link.o __mac_get_pid.o __mac_get_proc.o __mac_set_fd.o __mac_set_file.o __mac_set_link.o __mac_set_proc.o __semctl.o __setugid.o __syscall.o __sysctl.o _exit.o _getlogin.o _umtx_lock.o _umtx_op.o _umtx_unlock.o accept.o access.o acct.o adjtime.o aio_cancel.o aio_error.o aio_read.o aio_return.o aio_suspend.o aio_waitcomplete.o aio_write.o audit.o auditctl.o auditon.o bind.o chdir.o chflags.o chmod.o chown.o chroot.o clock_getres.o clock_gettime.o clock_settime.o close.o connect.o dup.o dup2.o eaccess.o execve.o extattr_delete_fd.o extattr_delete_file.o extattr_delete_link.o extattr_get_fd.o extattr_get_file.o extattr_get_link.o extattr_list_fd.o extattr_list_file.o extattr_list_link.o extattr_set_fd.o extattr_set_file.o extattr_set_link.o extattrctl.o fchdir.o fchflags.o fchmod.o fchown.o fcntl.o fhopen.o fhstat.o fhstatfs.o flock.o fork.o fpathconf.o fstat.o fstatfs.o fsync.o futimes.o getaudit.o getaudit_addr.o getauid.o getdents.o getdirentries.o getdtablesize.o getegid.o geteuid.o getfh.o getfsstat.o getgid.o getgroups.o getitimer.o getpeername.o getpgid.o getpgrp.o getpid.o getppid.o getpriority.o getresgid.o getresuid.o getrlimit.o getrusage.o getsid.o getsockname.o getsockopt.o gettimeofday.o getuid.o ioctl.o issetugid.o jail.o jail_attach.o kenv.o kevent.o kill.o kldfind.o kldfirstmod.o kldload.o kldnext.o kldstat.o kldsym.o kldunload.o kldunloadf.o kqueue.o kse_create.o kse_exit.o kse_release.o kse_switchin.o kse_thr_interrupt.o kse_wakeup.o ksem_close.o ksem_destroy.o ksem_getvalue.o ksem_init.o ksem_open.o ksem_post.o ksem_timedwait.o ksem_trywait.o ksem_unlink.o ksem_wait.o ktrace.o lchflags.o lchmod.o lchown.o lgetfh.o link.o lio_listio.o listen.o lstat.o lutimes.o mac_syscall.o madvise.o mincore.o minherit.o mkdir.o mkfifo.o mknod.o mlock.o mlockall.o modfind.o modfnext.o modnext.o modstat.o mount.o mprotect.o msgctl.o msgget.o msgrcv.o msgsnd.o msgsys.o msync.o munlock.o munlockall.o munmap.o nanosleep.o netbsd_lchown.o netbsd_msync.o nfsclnt.o nfssvc.o nfstat.o nlstat.o nmount.o nstat.o ntp_adjtime.o ntp_gettime.o open.o pathconf.o poll.o preadv.o profil.o pwritev.o quotactl.o read.o readlink.o readv.o recvfrom.o recvmsg.o rename.o revoke.o rfork.o rmdir.o rtprio.o sched_get_priority_max.o sched_get_priority_min.o sched_getparam.o sched_getscheduler.o sched_rr_get_interval.o sched_setparam.o sched_setscheduler.o sched_yield.o select.o semget.o semop.o semsys.o sendfile.o sendmsg.o sendto.o setaudit.o setaudit_addr.o setauid.o setcontext.o setegid.o seteuid.o setgid.o setgroups.o setitimer.o setpgid.o setpriority.o setregid.o setresgid.o setresuid.o setreuid.o setrlimit.o setsid.o setsockopt.o settimeofday.o setuid.o shmat.o shmctl.o shmdt.o shmget.o shmsys.o shutdown.o sigaction.o sigaltstack.o sigpending.o sigprocmask.o sigsuspend.o sigtimedwait.o sigwait.o sigwaitinfo.o socket.o socketpair.o stat.o statfs.o swapcontext.o swapoff.o swapon.o symlink.o sync.o sysarch.o thr_create.o thr_exit.o thr_kill.o thr_new.o thr_self.o thr_suspend.o thr_wake.o umask.o undelete.o unlink.o unmount.o utimes.o utrace.o uuidgen.o vadvise.o wait4.o write.o writev.o

all: $(OBJS)

# Compile the source files
.cc.o:
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<
 
.cc.s:
	$(CXX) $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

.c.s:
	$(CC) $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.S.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS)

