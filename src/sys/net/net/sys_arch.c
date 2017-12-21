/*
 * Copyright (c) 2001, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author:     Adam Dunkels      <adam@sics.se>
 */

#include <sys/types.h>
#include <ubixos/sched.h>
#include <ubixos/ubthread.h>
#include <ubixos/kpanic.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>

#include "net/debug.h"
#include "net/sys.h"
#include "net/opt.h"
#include "net/stats.h"
#include <net/arch/sys_arch.h>

#include <ubixos/spinlock.h>

#define ERR_NOT_READY 0
#define ERR_TIMED_OUT 1

static struct timeval starttime;
static spinLock_t netThreadSpinlock = SPIN_LOCK_INITIALIZER;
static struct sys_thread *threads = 0x0;

struct sys_mbox *sys_mbox_new_dead() {
  struct sys_mbox *mbox;

  mbox = kmalloc(sizeof(struct sys_mbox));
  memset(mbox, 0x0, sizeof(struct sys_mbox));
  mbox->first = mbox->last = 0;
  mbox->mail = sys_sem_new_(0);
  mbox->mutex = sys_sem_new_(1);

  return (mbox);
}

err_t sys_mbox_new(sys_mbox_t *mbox, int size) {
  LWIP_ASSERT("mbox null", mbox);
  mbox->first = 0;
  mbox->last = 0;
  mbox->mail = sys_sem_new_(0);
  mbox->mutex = sys_sem_new_(1);
  ubthread_mutex_init(mbox->lock, NULL);
  return (ERR_OK);
}


sys_thread_t sys_thread_new(const char *name, void (*thread)(void *arg), void *arg, int stacksize, int prio) {
  //void sys_thread_new(void (*function)(void), void *arg) {
  struct sys_thread *new_thread = 0x0;
  //struct thread_start_param *thread_param;


	LWIP_ASSERT("Non-positive prio", prio > 0);
	LWIP_ASSERT("Prio is too big", prio < 20);

  kprintf("sys_thread: [0x%X]\n", sizeof(struct sys_thread));

  new_thread = kmalloc(sizeof(struct sys_thread));
  memset(new_thread, 0x0, sizeof(struct sys_thread));
  kprintf("THREAD: [0x%X]\n", new_thread);

  spinLock(&netThreadSpinlock);
  new_thread->next = threads;
  new_thread->timeouts.next = NULL;
  new_thread->ubthread = 0x0;
  threads = new_thread;
  spinUnlock(&netThreadSpinlock);

  /*
   thread_param = kmalloc(sizeof(struct thread_start_param));

   thread_param->function = function;
   thread_param->arg = arg;
   thread_param->thread = thread;
   */
  //execThread((void *)function,0x0,0x0);
  kprintf("thread->ubthread: [0x%X]\n", new_thread->ubthread);
  if (ubthread_create(&new_thread->ubthread, 0x0, (void *) (thread), arg) != 0x0) {
    kpanic("sys_thread_new: ubthread_create");
  }
  kprintf("thread->ubthread: [0x%X]\n", new_thread->ubthread);
  return (new_thread);
}

err_t sys_sem_new(sys_sem_t *sem, uint8_t count) {
  //struct sys_sem *sem;

  sem = kmalloc(sizeof(struct sys_sem));
  memset(sem, 0x0, sizeof(struct sys_sem));
  sem->c = count;

  ubthread_cond_init(&(sem->cond), NULL);
  ubthread_mutex_init(&(sem->mutex), NULL);

  return (ERR_OK);
}

void sys_sem_free(struct sys_sem *sem) {
  if (sem != SYS_SEM_NULL) {
  ubthread_cond_destroy(&(sem->cond));
  ubthread_mutex_destroy(&(sem->mutex));
  kfree(sem);
  }
}

uint32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, uint32_t timeout) {
  //LTRACE_ENTRY;
  status_t res;

  //lk_time_t start = current_time();

  uint32_t start = sys_now();

  res = sem_timedwait(&mbox->full, timeout ? timeout : INFINITE_TIME);
  if (res == ERR_TIMED_OUT) {
    //LTRACE_EXIT;
   return SYS_ARCH_TIMEOUT; //timeout ? SYS_ARCH_TIMEOUT : 0;
  }
	
  mutex_acquire(&mbox->lock);

  *msg = mbox->queue[mbox->tail];
  mbox->tail = (mbox->tail + 1) % mbox->size;

  mutex_release(&mbox->lock);
  sem_post(&mbox->empty);

  //LTRACE_EXIT;
  return sys_now() - start;
}

#ifdef _BALLS
uint32_t sys_arch_mbox_fetch_dead(struct sys_mbox *mbox, void **msg, uint32_t timeout) {
  uint16_t time = 1;

  /* The mutex lock is quick so we don't bother with the timeout
   stuff here. */
  //kprintf("sem wait0");
  sys_arch_sem_wait(mbox->mutex, 0);
  //kprintf("sem wait1");

  while (mbox->first == mbox->last) {
    //kprintf("sem wait2");
    sys_sem_signal(mbox->mutex);
    //kprintf("sem wait3");

    /* We block while waiting for a mail to arrive in the mailbox. We
     must be prepared to timeout. */
    if (timeout != 0) {
     // kprintf("sem wait4");
      time = sys_arch_sem_wait(mbox->mail, timeout);
      //kprintf("sem wait5");

      /* If time == 0, the sem_wait timed out, and we return 0. */
      if (time == 0) {
        return 0;
      }
    }
    else {
      //kprintf("sem wait6");
      sys_arch_sem_wait(mbox->mail, 0);
      //kprintf("sem wait7");
    }

    //kprintf("sem wait8");
    sys_arch_sem_wait(mbox->mutex, 0);
   // kprintf("sem wait9");
  }
  //kprintf("sem wait10");

  if (msg != NULL) {
    //kprintf("sys_mbox_fetch: mbox %p msg %p\n", mbox, *msg);
    *msg = mbox->msgs[mbox->first];
  }

  mbox->first++;
  if (mbox->first == SYS_MBOX_SIZE) {
    mbox->first = 0;
  }

  sys_sem_signal(mbox->mutex);

  return (time);
}
#endif

//#define UMAX(a, b)      ((a) > (b) ? (a) : (b))


struct sys_mbox_msg {
  struct sys_mbox_msg *next;
  void *msg;
};

#define SYS_MBOX_SIZE 100

/*
struct sys_mbox {
  uint16_t first, last;
  void *msgs[SYS_MBOX_SIZE];
  struct sys_sem *mail;
  struct sys_sem *mutex;
};
*/

/*
struct sys_sem {
  unsigned int c;
  ubthread_cond_t cond;
  ubthread_mutex_t mutex;
};
*/

static uint16_t cond_wait(ubthread_cond_t *cond, ubthread_mutex_t *mutex, uint16_t timeout);


struct thread_start_param {
  struct sys_thread *thread;
  void (*function)(void *);
  void *arg;
};

/*
 static void *thread_start(void *arg) {
 struct thread_start_param *tp = arg;
 tp->thread->ubthread = ubthread_self();
 tp->function(tp->arg);
 kfree(tp);
 return(NULL);
 }
*/

void sys_mbox_free(sys_mbox_t *mbox) {
	free(mbox->queue);
	mbox->queue = NULL;
}

#ifdef _BALLS
void sys_mbox_free(struct sys_mbox *mbox) {
  if (mbox != SYS_MBOX_NULL) {
    sys_sem_wait(mbox->mutex);
    sys_sem_free_(mbox->mail);
    sys_sem_free_(mbox->mutex);
    mbox->mail = mbox->mutex = NULL;
    kfree(mbox);
  }
}
#endif


void sys_mbox_post(sys_mbox_t * mbox, void *msg)
{
	sem_wait(&mbox->empty);
	mutex_acquire(&mbox->lock);

	mbox->queue[mbox->head] = msg;
	mbox->head = (mbox->head + 1) % mbox->size;

	mutex_release(&mbox->lock);
	sem_post(&mbox->full);
}

#ifdef _BALLS
void sys_mbox_post(struct sys_mbox *mbox, void *msg) {
  uInt8 first;

  sys_sem_wait(mbox->mutex);

  //kprintf("sys_mbox_post: mbox %p msg %p\n", mbox, msg);

  mbox->msgs[mbox->last] = msg;

  if (mbox->last == mbox->first)
    first = 1;
  else
    first = 0;

  mbox->last++;

  if (mbox->last == SYS_MBOX_SIZE)
    mbox->last = 0;


  if (first)
    sys_sem_signal(mbox->mail);

  sys_sem_signal(mbox->mutex);
}
#endif


static uint16_t cond_wait(ubthread_cond_t *cond, ubthread_mutex_t *mutex, uint16_t timeout) {
  unsigned int tdiff;
  unsigned long sec, usec;
  struct timeval rtime1, rtime2;
  struct timespec ts;
  struct timezone tz;
  int retval;

  if (timeout > 0) {
    /* Get a timestamp and add the timeout value. */
    gettimeofday(&rtime1, &tz);
    sec = rtime1.tv_sec;
    usec = rtime1.tv_usec;
    usec += timeout % 1000 * 1000;
    sec += (int) (timeout / 1000) + (int) (usec / 1000000);
    usec = usec % 1000000;
    ts.tv_nsec = usec * 1000;
    ts.tv_sec = sec;

    retval = ubthread_cond_timedwait(cond, mutex, &ts);
    if (retval == ETIMEDOUT) {
      return 0;
    }
    else {
      /* Calculate for how long we waited for the cond. */
      gettimeofday(&rtime2, &tz);
      tdiff = (rtime2.tv_sec - rtime1.tv_sec) * 1000 + (rtime2.tv_usec - rtime1.tv_usec) / 1000;
      if (tdiff == 0) {
        return 1;
      }
      return tdiff;
    }
  }
  else {
    ubthread_cond_wait(cond, mutex);
    return 0;
  }
}

uint32_t sys_arch_sem_wait(struct sys_sem *sem, uint32_t timeout) {
  uint16_t time = 1;
  ubthread_mutex_lock(&(sem->mutex));
  while (sem->c <= 0) {
    if (timeout > 0) {
      time = cond_wait(&(sem->cond), &(sem->mutex), timeout);
      if (time == 0) {
        ubthread_mutex_unlock(&(sem->mutex));
        return 0;
      }
    }
    else {
      cond_wait(&(sem->cond), &(sem->mutex), 0);
    }
  }
  sem->c--;
  ubthread_mutex_unlock(&(sem->mutex));
  return (time);
}

void sys_sem_signal(struct sys_sem *sem) {
  //kprintf("HERE: %i:0x%X", _current->id,&(sem->mutex));

  ubthread_mutex_lock(&(sem->mutex));

  sem->c++;
  if (sem->c > 1)
    sem->c = 1;

  ubthread_cond_signal(&(sem->cond));
  ubthread_mutex_unlock(&(sem->mutex));
}


void sys_init() {
  struct timezone tz;
  gettimeofday(&starttime, &tz);
}

static struct sys_thread *current_thread(void) {
  struct sys_thread *st;
  kTask_t *pt;
  pt = ubthread_self();
  //kprintf("SL: %i-0x%X]", _current->id, &netThreadSpinlock);
  spinLock(&netThreadSpinlock);
  for (st = threads; st != NULL; st = st->next) {
    if (st->ubthread == pt) {
      //kprintf("SUL: %i-0x%X]", _current->id, &netThreadSpinlock);
      spinUnlock(&netThreadSpinlock);
      return st;
    }
  }
  //kprintf("SUL: %i-0x%X]", _current->id, &netThreadSpinlock);
  spinUnlock(&netThreadSpinlock);
  kprintf("sys: current_thread: could not find current thread!\n");
  kprintf("This is due to a race condition in the LinuxThreads\n");
  kprintf("ubthreads implementation. Start the program again.\n");

  kpanic("ABORT");
  return (0x0);
}

struct sys_timeouts *sys_arch_timeouts(void) {
  struct sys_thread *thread;
  thread = current_thread();
  return (&thread->timeouts);
}

unsigned long sys_unix_now() {
  struct timeval tv;
  struct timezone tz;
  long sec, usec;
  unsigned long msec;

  gettimeofday(&tv, &tz);

  sec = tv.tv_sec - starttime.tv_sec;
  usec = tv.tv_usec - starttime.tv_usec;
  msec = sec * 1000 + usec / 1000;
  return msec;
}

uint32_t sys_now() {
  return(sys_unix_now());
}

int sys_mbox_valid(sys_mbox_t *mbox)
{
	return mbox->msgs != NULL;
}

err_t sys_mbox_trypost(sys_mbox_t * mbox, void *msg) {
	status_t res;

	res = sem_trywait(&mbox->empty);
	if (res == ERR_NOT_READY)
		return ERR_TIMEOUT;
	
	mutex_acquire(&mbox->lock);

	mbox->queue[mbox->head] = msg;
	mbox->head = (mbox->head + 1) % mbox->size;

	mutex_release(&mbox->lock);
	sem_post(&mbox->full);

	return ERR_OK;
}
