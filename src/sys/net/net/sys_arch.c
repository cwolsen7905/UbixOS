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

/* Get Definitions For These */
#define ERR_NOT_READY 0
#define ERR_TIMED_OUT 1
#define INFINITE_TIME 0

static struct timeval starttime;
static struct spinLock netThreadSpinlock = SPIN_LOCK_INITIALIZER;
static struct sys_thread *threads = 0x0;

static uint16_t cond_wait(ubthread_cond_t *cond, ubthread_mutex_t *mutex, uint16_t timeout);

/* sys_arch layer initializer */
void sys_init() {
  struct timezone tz;
  gettimeofday(&starttime, &tz);
}

/* Create a new semaphore */
err_t sys_sem_new(sys_sem_t *sem, uint8_t count) {
  sem->signaled = count;

  ubthread_cond_init(&(sem->cond), NULL);
  ubthread_mutex_init(&(sem->mutex), NULL);

  return (ERR_OK);
}

/* Deallocate semaphore */
void sys_sem_free(sys_sem_t *sem) {
  ubthread_cond_destroy(&(sem->cond));
  ubthread_mutex_destroy(&(sem->mutex));
  //MrOlsen maybe not here
  kfree(sem);
}

/* Signal semaphore */
void sys_sem_signal(sys_sem_t *sem) {
  kprintf("L1");
  ubthread_mutex_lock(&(sem->mutex));

    sem->signaled++;
    if (sem->signaled > 1)
      sem->signaled = 1;

    //ubthread_cond_signal(&(sem->cond));
    ubthread_mutex_unlock(&(sem->mutex));
}

uint32_t _sys_arch_sem_wait(struct sys_sem *sem, uint32_t timeout) {
  uint32_t time = sys_now();

  ubthread_mutex_lock(&(sem->mutex));

  while (sem->signaled <= 0) {
    if (timeout > 0) {
      time = cond_wait(&(sem->cond), &(sem->mutex), timeout);
      time = 0;
//kprintf("%s:%i\n", __FILE__, __LINE__);
      if (time == 0) {
//kprintf("%s:%i\n", __FILE__, __LINE__);
        ubthread_mutex_unlock(&(sem->mutex));
//kprintf("%s:%i\n", __FILE__, __LINE__);
        return(0);
      }
    }
    else {
//kprintf("%s:%i\n", __FILE__, __LINE__);
      time = cond_wait(&(sem->cond), &(sem->mutex), 0);
     timeout = 1;
//kprintf("%s:%i\n", __FILE__, __LINE__);
    }
  }
  sem->signaled--;
  //kprintf("L3");
  ubthread_mutex_lock(&(sem->mutex));
  kprintf("L3.1");
  return (sys_now() - time);
}

uint32_t sys_arch_sem_wait(struct sys_sem *sem, uint32_t timeout) {
  uint32_t time = sys_now();
  uint32_t ret = 0x0;

  if (sem->signaled > 0) {
    ubthread_mutex_lock(&(sem->mutex));
    sem->signaled--;
    ubthread_mutex_unlock(&(sem->mutex));
    return (ret);
  }

  while (sem->signaled <= 0) {
    if (timeout > 0) {
      sched_yield();
      ret = sys_now() - time;
    }
    else {
      sched_yield();
      ret = SYS_ARCH_TIMEOUT;
    }
  }
  ubthread_mutex_lock(&(sem->mutex));
  sem->signaled--;
  ubthread_mutex_unlock(&(sem->mutex));
  return (ret);
}

int sys_sem_valid(sys_sem_t *sem) {
  if (sem == 0)
    return 1;
  else
    return 0;
}

void sys_sem_set_invalid(sys_sem_t *sem) {
  kprintf("NEED TO DO THIS");
}

err_t sys_mutex_new(sys_mutex_t *mutex) {
  ubthread_mutex_init(&(mutex->mutex), NULL);
  return ERR_OK;
}

void sys_mutex_free(sys_mutex_t *mutex) {
  ubthread_mutex_destroy(&(mutex->mutex));
}

void sys_mutex_lock(sys_mutex_t *mutex) {
  kprintf("L4.0");
   ubthread_mutex_lock(&(mutex->mutex)) ;
  kprintf("L4.1");
}

void sys_mutex_unlock(sys_mutex_t *mutex) {
  ubthread_mutex_unlock(&(mutex->mutex)) ;
}

err_t sys_mbox_new(sys_mbox_t *mbox, int size) {
  LWIP_ASSERT("mbox null", mbox);
  ubthread_mutex_init(mbox->lock, NULL);

  mbox->head = 0;
  mbox->tail = 0;
  mbox->size = size;

  sys_sem_new(mbox->empty, size);
  sys_sem_new(mbox->full, 0);

  mbox->queue = kmalloc(sizeof(void *) * size);//calloc(size, sizeof(void *));

  if (!mbox->queue) {
    kprintf("WTF: [%i]", size);
    return ERR_MEM;
  }

  return (ERR_OK);
}

void sys_mbox_free(sys_mbox_t *mbox) {
  kfree(mbox->queue);
  mbox->queue = NULL;
}

void sys_mbox_post(sys_mbox_t * mbox, void *msg) {
  sys_arch_sem_wait(&(mbox->empty), 0);
  kprintf("L5");
  ubthread_mutex_lock(&mbox->lock);

  mbox->queue[mbox->head] = msg;
  mbox->head = (mbox->head + 1) % mbox->size;

  ubthread_mutex_unlock(&mbox->lock);
  //sem_post(&mbox->full);
  sys_sem_signal(&(mbox->full));
}

err_t sys_mbox_trypost(sys_mbox_t * mbox, void *msg) {
uint32_t res;

kprintf("%s:%i\n", __FILE__, __LINE__);
/* SHOULD BE TRY WAIT */
res = sys_arch_sem_wait(&mbox->empty, 0x0);
kprintf("%s:%i\n", __FILE__, __LINE__);
if (res == ERR_NOT_READY)
return ERR_TIMEOUT;
kprintf("%s:%i\n", __FILE__, __LINE__);

  kprintf("L6");
ubthread_mutex_lock(&mbox->lock);

mbox->queue[mbox->head] = msg;
mbox->head = (mbox->head + 1) % mbox->size;

ubthread_mutex_unlock(&mbox->lock);
sys_sem_signal(&(mbox->full));
//sem_post(&mbox->full);

return ERR_OK;
}

uint32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, uint32_t timeout) {

  //status_t res;
  uint32_t res;

  //lk_time_t start = current_time();

  uint32_t start = sys_now();
  kprintf("Timeout1: %i]", timeout);
  res = sys_arch_sem_wait(&(mbox->full), timeout ? timeout : INFINITE_TIME);
  kprintf("Timeout2: %i]", timeout);
  //res = sem_timedwait(&mbox->full, timeout ? timeout : INFINITE_TIME);
  if (res == ERR_TIMED_OUT) {
    //LTRACE_EXIT;
    return SYS_ARCH_TIMEOUT; //timeout ? SYS_ARCH_TIMEOUT : 0;
  }

  kprintf("L7");
  ubthread_mutex_lock(&mbox->lock);

  *msg = mbox->queue[mbox->tail];
  mbox->tail = (mbox->tail + 1) % mbox->size;

  ubthread_mutex_unlock(&mbox->lock);
  sys_sem_signal(&(mbox->empty));
  //sem_post(&mbox->empty);

  return sys_now() - start;
}

uint32_t sys_arch_mbox_tryfetch(sys_mbox_t * mbox, void **msg) {
//LTRACE_ENTRY;

//status_t res;
  uint32_t res;

res = sys_arch_sem_wait(&(mbox->full), 0x0);
//res = sem_trywait(&mbox->full);
if (res == ERR_NOT_READY) {
//LTRACE_EXIT;
return SYS_MBOX_EMPTY;
}

  kprintf("L8");
ubthread_mutex_lock(&mbox->lock);

*msg = mbox->queue[mbox->tail];
mbox->tail = (mbox->tail + 1) % mbox->size;

ubthread_mutex_unlock(&mbox->lock);
sys_sem_signal(&(mbox->empty));
//sem_post(&mbox->empty);

//LTRACE_EXIT;
return 0;
}


int sys_mbox_valid(sys_mbox_t *mbox) {
  return mbox->queue != NULL;
}

void sys_mbox_set_invalid(sys_mbox_t *mbox) {
}

sys_thread_t sys_thread_new(const char *name, void (*thread)(void *arg), void *arg, int stacksize, int prio) {
  //void sys_thread_new(void (*function)(void), void *arg) {
  struct sys_thread *new_thread = 0x0;
  //struct thread_start_param *thread_param;
  prio = 1;
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

/* OLD */

struct thread_start_param {
  struct sys_thread *thread;
  void (*function)(void *);
  void *arg;
};


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
  return (sys_unix_now());
}

#ifdef _BALLS

#define UMAX(a, b)      ((a) > (b) ? (a) : (b))

struct sys_mbox_msg {
  struct sys_mbox_msg *next;
  void *msg;
};

struct sys_mbox *sys_mbox_new_BALLS() {
  struct sys_mbox *mbox;

  mbox = kmalloc(sizeof(struct sys_mbox));
  memset(mbox, 0x0, sizeof(struct sys_mbox));
  mbox->first = mbox->last = 0;
  mbox->mail = sys_sem_new_(0);
  mbox->mutex = sys_sem_new_(1);

  return (mbox);
}

uint32_t sys_arch_mbox_fetch_BALLS(struct sys_mbox *mbox, void **msg, uint32_t timeout) {
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

void sys_mbox_free_BALLS(struct sys_mbox *mbox) {
  if (mbox != SYS_MBOX_NULL) {
    sys_sem_wait(mbox->mutex);
    sys_sem_free_(mbox->mail);
    sys_sem_free_(mbox->mutex);
    mbox->mail = mbox->mutex = NULL;
    kfree(mbox);
  }
}

void sys_mbox_post_BALLS(struct sys_mbox *mbox, void *msg) {
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
