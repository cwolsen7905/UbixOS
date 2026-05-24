/*
 * FreeBSD has no kernel pause(2) — implement via sigsuspend with the
 * current mask, which unblocks no additional signals but still sleeps
 * until any already-unblocked signal arrives.
 */
#include <signal.h>
#include <unistd.h>
#include "syscall.h"

int pause(void)
{
	sigset_t mask;
	sigemptyset(&mask);
	return sigsuspend(&mask);
}
