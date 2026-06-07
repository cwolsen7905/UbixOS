/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Machine-dependent userland TLS install — the arch hook behind the generic
 * set_thread_area(2) syscall.  Keeps i386 segment/LDT assembly out of generic
 * code so a 64-bit / arm64 build compiles; each arch supplies the impl in its
 * arch dir (i386 → LDT[1] + %gs; aarch64 → TPIDR_EL0).
 */
#ifndef _MACHINE_TLS_H
#define _MACHINE_TLS_H

#include <sys/types.h>

struct thread;

/**
 * Install @base as the calling thread's userland TLS self-pointer (the value
 * userland reads at %gs:0 on i386, or via TPIDR_EL0 on aarch64).  @td is the
 * current thread (so the arch impl can patch the return-to-user register state,
 * e.g. i386's saved %gs).  @base is the pointer recorded in kTask_t.tls_base.
 */
void machine_set_tls(struct thread *td, uintptr_t base);

#endif /* _MACHINE_TLS_H */
