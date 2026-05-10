#ifndef _SYS_UCONTEXT_H_
#define _SYS_UCONTEXT_H_

#include <sys/signal.h>
#include <sys/types.h>

/* i386 general register set — Linux ABI layout used by TCC backtrace code */
#define NGREG	19

typedef int	greg_t;
typedef greg_t	gregset_t[NGREG];

/* Indices into gregset_t */
#define REG_GS		0
#define REG_FS		1
#define REG_ES		2
#define REG_DS		3
#define REG_EDI		4
#define REG_ESI		5
#define REG_EBP		6
#define REG_ESP		7
#define REG_EBX		8
#define REG_EDX		9
#define REG_ECX		10
#define REG_EAX		11
#define REG_TRAPNO	12
#define REG_ERR		13
#define REG_EIP		14
#define REG_CS		15
#define REG_EFL		16
#define REG_UESP	17
#define REG_SS		18

typedef struct {
	gregset_t	gregs;
} mcontext_t;

typedef struct ucontext {
	unsigned int		uc_flags;
	struct ucontext		*uc_link;
	sigset_t		uc_sigmask;
	mcontext_t		uc_mcontext;
} ucontext_t;

#endif /* !_SYS_UCONTEXT_H_ */
