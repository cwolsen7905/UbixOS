/**
 * signal.h
 */

#ifndef _SIGNAL_H
#define _SIGNAL_H

typedef int sig_atomic_t;

#define	SIGHUP	1		/* hangup */
#define	SIGINT	2		/* interrupt (DEL) */
#define	SIGQUIT	3		/* quit (ASCII FS) */
#define	SIGILL	4		/* illegal instruction */
#define	SIGTRAP	5		/* trace trap (not reset when caught) */
#define	SIGABRT	6		/* IOT instruction */
#define	SIGBUS	7
#define	SIGEMT	7
#define	SIGFPE	8		/* floating point exception */
#define	SIGKILL	9		/* kill (cannot be caught or ignored) */
#define	SIGUSR1	10		/* user defined signal 1 */
#define	SIGSEGV	11		/* segmentation violation */
#define	SIGUSR2	12		/* user defined signal 2 */
#define	SIGPIPE	13		/* write on a pipe with no one to read it */
#define	SIGALRM	14		/* alarm clock */
#define	SIGTERM	15		/* software termination signal from kill */

/**
 * For POSIX compliance
 */
#define	SIGCHLD	17		/* child process terminated or stopped */
#define	SIGCONT	18		/* continue if stopped */
#define	SIGSTOP	19		/* stop signal */
#define	SIGTSTP	20		/* interactive stop signal */
#define	SIGTTIN	21		/* background process requesting read */
#define	SIGTTOU	22		/* background process requesting write */

#if 0
#define	SIG_ERR	((__sighandler_t) -1)	/* error return */
#define	SIG_DFL	((__sighandler_t) 0)		/* default signal handling */
#define	SIG_IGN	((__sighandler_t) 1)		/* ignore signal */
#endif /* 0 */

#define	_NSIG		23

#if 0
int raise(int sig);
__sighandler_t signal(int sig, __sighandler_t func);
int kill(pid_t pid, int sig);
#endif /* 0 */

#endif /* _SIGNAL_H */

