/**
 * fcntl.h
 *
 * access constants for _open. Note that this file is not complete
 * and should not be used (yet)
 */

#ifndef _SIGNAL_H
#define _SIGNAL_H

/**
 * The actual signal values. Using other values with signal
 * produces a SIG_ERR return value.
 *
 * NOTE:	SIGINT is produced when the user presses Ctrl-C
 * 		SIGILL has not been tested
 * 		SIGFPE 
 * 		SIGSEGV
 * 		SIGTERM comes from what kind of termination request exactly?
 * 		SIGBREAK is produced by pressing Ctrl-Break
 * 		SIGABRT is produced by calling abort
 */
#define	SIGINT		2		/* Interactive attention */
#define	SIGILL		4		/* Illegal instruction */
#define	SIGFPE		8		/* Floating point error */
#define	SIGSEGV		11		/* Segmentation violation */
#define	SIGTERM		15		/* Termination request */
#define	SIGBREAK		21		/* Ctrl-Break */
#define	SIGABRT		22		/* Abnormal termination (abort) */

#define	NSIG			23		/* maximum signal number + 1 */

#ifndef RC_INVOKED
#ifndef _SIG_ATOMIC_T_DEFINED
typedef int sig_atomic_t;
#define _SIG_ATOMIC_T_DEFINED
#endif /* ! _SIG_ATOMIC_T_DEFINED */

/**
 * The prototypes (below) are the easy part. The hard part is figuring
 * out what signals are available and what numbers they are assigned
 * along with appropriate values of SIG_DFL and SIG_IGN.
 */

/**
 * A pointer to a signal handler function. A signal handler takes a
 * single int, which is the signal it handles.
 */
typedef void (*__p_sig_fn_t)(int);

/**
 * These are special values of signal handler pointers which are
 * used to send a signal to the default handler (SIG_DFL), ignore
 * the signal (SIG_IGN), or indicate an error return (SIG_ERR).
 */
#define	SIG_DFL	((__p_sig_fn_t) 0)
#define	SIG_IGN	((__p_sig_fn_t) 1)
#define	SIG_ERR ((__p_sig_fn_t) -1)

#ifdef	__cplusplus
extern "C" {
#endif

/**
 * Call signal to set the signal handler for signal sig to the
 * function pointed to by handler. Returns a pointer to the
 * previous handler, or SIG_ERR if an error occurs. Initially
 * unhandled signals defined above will return SIG_DFL.
 */
_CRTIMP __p_sig_fn_t __cdecl signal(int, __p_sig_fn_t);

/**
 * Raise the signal indicated by sig. Returns non-zero on success.
 */
_CRTIMP int __cdecl raise (int);

#ifdef	__cplusplus
}
#endif

#endif /* ! RC_INVOKED */
#endif /* ! _SIGNAL_H */

