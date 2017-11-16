# $Id: Makefile 89 2016-01-12 00:20:40Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../../Makefile.inc
include ../../Makefile.inc
include ../Makefile.inc

INCLUDES += -I../locale/ -I.

#Objects
OBJS = uthread_accept.o uthread_acl_aclcheck_fd.o uthread_acl_delete_fd.o uthread_acl_get_fd.o uthread_acl_set_fd.o uthread_aio_suspend.o uthread_atfork.o uthread_attr_destroy.o uthread_attr_get_np.o uthread_attr_getdetachstate.o uthread_attr_getguardsize.o uthread_attr_getinheritsched.o uthread_attr_getschedparam.o uthread_attr_getschedpolicy.o uthread_attr_getscope.o uthread_attr_getstack.o uthread_attr_getstackaddr.o uthread_attr_getstacksize.o uthread_attr_init.o uthread_attr_setcreatesuspend_np.o uthread_attr_setdetachstate.o uthread_attr_setguardsize.o uthread_attr_setinheritsched.o uthread_attr_setschedparam.o uthread_attr_setschedpolicy.o uthread_attr_setscope.o uthread_attr_setstack.o uthread_attr_setstackaddr.o uthread_attr_setstacksize.o uthread_autoinit.o uthread_bind.o uthread_cancel.o uthread_clean.o uthread_close.o uthread_concurrency.o uthread_cond.o uthread_condattr_destroy.o uthread_condattr_init.o uthread_connect.o uthread_creat.o uthread_create.o uthread_detach.o uthread_dup.o uthread_dup2.o uthread_equal.o uthread_execve.o uthread_exit.o uthread_fchflags.o uthread_fchmod.o uthread_fchown.o uthread_fcntl.o uthread_fd.o uthread_file.o uthread_find_thread.o uthread_flock.o uthread_fork.o uthread_fpathconf.o uthread_fstat.o uthread_fstatfs.o uthread_fsync.o uthread_gc.o uthread_getdirentries.o uthread_getpeername.o uthread_getprio.o uthread_getschedparam.o uthread_getsockname.o uthread_getsockopt.o uthread_info.o uthread_init.o uthread_ioctl.o uthread_jmp.o uthread_join.o uthread_kern.o uthread_kevent.o uthread_kill.o uthread_kqueue.o uthread_listen.o uthread_main_np.o uthread_mattr_init.o uthread_mattr_kind_np.o uthread_msync.o uthread_multi_np.o uthread_mutex.o uthread_mutex_prioceiling.o uthread_mutex_protocol.o uthread_mutexattr_destroy.o uthread_nanosleep.o uthread_once.o uthread_open.o uthread_pause.o uthread_pipe.o uthread_poll.o uthread_priority_queue.o uthread_pselect.o uthread_read.o uthread_readv.o uthread_recvfrom.o uthread_recvmsg.o uthread_resume_np.o uthread_rwlock.o uthread_rwlockattr.o uthread_select.o uthread_self.o uthread_sem.o uthread_sendfile.o uthread_sendmsg.o uthread_sendto.o uthread_seterrno.o uthread_setprio.o uthread_setschedparam.o uthread_setsockopt.o uthread_shutdown.o uthread_sig.o uthread_sigaction.o uthread_sigmask.o uthread_sigpending.o uthread_sigprocmask.o uthread_sigsuspend.o uthread_sigwait.o uthread_single_np.o uthread_sleep.o uthread_socket.o uthread_socketpair.o uthread_spec.o uthread_spinlock.o uthread_stack.o uthread_suspend_np.o uthread_switch_np.o uthread_system.o uthread_tcdrain.o uthread_vfork.o uthread_wait.o uthread_wait4.o uthread_waitpid.o uthread_write.o uthread_writev.o uthread_yield.o

all: $(OBJS)

# Compile the source files
.cc.o:
	$(CXX) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -c -o $@ $<
 
.cc.s:
	$(CXX) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -S -o $@ $<
 
.c.o:
	$(CC) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -c $<

.c.s:
	$(CC) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -S -o $@ $<
 
.S.o:
	$(CC) $(CFLAGS) -Wall -nostdlib -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS)

