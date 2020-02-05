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

/* All these must be converted to be done atomically */

#include <ubixos/ubthread.h>
#include <ubixos/exec.h>
#include <ubixos/sched.h>
#include <ubixos/time.h>
#include <ubixos/spinlock.h>
#include <ubixos/vitals.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <ubixos/kpanic.h>
#include <sys/stdatomic.h>

struct ubthread_cond_list *conds = 0x0;
struct ubthread_mutex_list *mutex = 0x0;

kTask_t* ubthread_self() {
    return (_current);
}

int ubthread_cond_init(ubthread_cond_t *cond, const uint32_t attr) {
    ubthread_cond_t ubcond = kmalloc(sizeof(struct ubthread_cond));
    memset(ubcond, 0x0, sizeof(struct ubthread_cond));

    ubcond->id = (int) cond;
    ubcond->lock = ATOMIC_VAR_INIT(0);

    *cond = ubcond;
    return (0x0);
}

int ubthread_mutex_init(ubthread_mutex_t *mutex, const uint32_t attr) {
    ubthread_mutex_t ubmutex = kmalloc(sizeof(struct ubthread_mutex));
    memset(ubmutex, 0x0, sizeof(struct ubthread_mutex));

    ubmutex->id = (int) mutex;
    ubmutex->lock = ATOMIC_VAR_INIT(0);

    *mutex = ubmutex;
    return (0x0);
}

int ubthread_cond_destroy(ubthread_cond_t *cond) {
    kfree(*cond);
    *cond = 0x0;
    return (0x0);
}

int ubthread_mutex_destroy(ubthread_mutex_t *mutex) {
    kfree(*mutex);
    *mutex = 0x0;
    return (0x0);
}

int ubthread_create(kTask_t **thread, const uInt32 *attr, void (*tproc)(void), void *arg) {
    *thread = (void*) execThread(tproc, 0x2000, arg);
    return (0x0);
}

int ubthread_mutex_lock(ubthread_mutex_t *mutex) {
    ubthread_mutex_t ubmutex = *mutex;

    if (ubmutex->lock == TRUE && ubmutex->pid == _current->id) {
        kprintf("Mutex Already Locked By This Thread");
        kpanic("WHY?");
        return (0x0);
    }

    while (1) {
        if (xchg_32(&ubmutex->lock, TRUE) == FALSE)
            break;

        while (ubmutex->lock == TRUE)
            sched_yield();
    }

    ubmutex->pid = _current->id;
    return (0x0);
}

int ubthread_mutex_unlock(ubthread_mutex_t *mutex) {
    ubthread_mutex_t ubmutex = *mutex;

    if (ubmutex->lock != TRUE)
        kpanic("NOT LOCKED?");

    if (ubmutex->pid != _current->id)
        kprintf("Trying To Unlock Mutex From No Locking Thread[%i - %i:0x%X]\n", ubmutex->pid, _current->id, *ubmutex);

    while (1) {
        if (xchg_32(&ubmutex->lock, FALSE) == TRUE)
            break;
        while (ubmutex->lock == FALSE)
            sched_yield();
    }

    ubmutex->pid = 0x0;
    return (0x0);
}

int ubthread_cond_timedwait(ubthread_cond_t *cond, ubthread_mutex_t *mutex, const struct timespec *abstime) {
    ubthread_cond_t ubcond = *cond;
    ubthread_mutex_t ubmutex = *mutex;

    uint32_t enterTime = systemVitals->sysUptime + 20;

    ubthread_mutex_unlock(mutex);

    while (enterTime > systemVitals->sysUptime) {
        if (ubcond->lock == FALSE)
            break;
        sched_yield();
    }

    ubthread_mutex_lock(mutex);

    return (0x0);
}

int ubthread_cond_wait(ubthread_cond_t *cond, ubthread_mutex_t *mutex) {
    ubthread_cond_t ubcond = *cond;
    ubthread_mutex_unlock(mutex);
    while (ubcond->lock == TRUE)
        sched_yield();
    ubthread_mutex_lock(mutex);
    return (0x0);
}

int ubthread_cond_signal(ubthread_cond_t *cond) {
    ubthread_cond_t ubcond = *cond;
    while (xchg_32(&ubcond->lock, FALSE))
        sched_yield();
    return (0x0);
}

int ubthread_cond_broadcast(ubthread_cond_t *cond) {
    ubthread_cond_t ubcond = *cond;
    while (xchg_32(&ubcond->lock, FALSE))
        sched_yield();
    return (0x0);
}

