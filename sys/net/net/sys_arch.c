#include <sys/types.h>
#include <ubixos/sched.h>
#include <ubixos/ubthread.h>
#include <ubixos/kpanic.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <sys/sysproto.h>
#include <sys/descrip.h>

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

static uint32_t cond_wait(ubthread_cond_t *cond, ubthread_mutex_t *mutex, uint32_t timeout);
static void sys_sem_free_internal(struct sys_sem *sem);

/* sys_arch layer initializer */
void sys_init() {
  struct timezone tz;
  gettimeofday(&starttime, &tz);
}

static struct sys_sem *sys_sem_new_internal(uint8_t count) {
  struct sys_sem *sem;

  sem = (struct sys_sem *) kmalloc(sizeof(struct sys_sem));
  if (sem != NULL) {
    sem->signaled = count;
    ubthread_cond_init(&(sem->cond), NULL);
    ubthread_mutex_init(&(sem->mutex), NULL);
  }
  return sem;
}

/* Create a new semaphore */
err_t sys_sem_new(sys_sem_t **sem, uint8_t count) {
  sys_sem_t *newSem = 0x0;

  newSem = kmalloc(sizeof(struct sys_sem));
  newSem->signaled = count;

  ubthread_cond_init(&(newSem->cond), NULL);
  ubthread_mutex_init(&(newSem->mutex), NULL);

  if (*sem != 0)
    kpanic("UH OH!");

  *sem = newSem;

  return (ERR_OK);
}

/* Deallocate semaphore */
void sys_sem_free(struct sys_sem **sem) {
  if ((sem != NULL) && (*sem != SYS_SEM_NULL)) {
    sys_sem_free_internal(*sem);
  }
}

/* Signal semaphore */
void sys_sem_signal(struct sys_sem **s) {
  struct sys_sem *sem;
  LWIP_ASSERT("invalid sem", (s != NULL) && (*s != NULL));
  sem = *s;

  ubthread_mutex_lock(&(sem->mutex));
  sem->signaled++;

  if (sem->signaled > 1) {
    sem->signaled = 1;
  }

  ubthread_cond_broadcast(&(sem->cond));
  ubthread_mutex_unlock(&(sem->mutex));
}

static void sys_sem_free_internal(struct sys_sem *sem) {
  ubthread_cond_destroy(&(sem->cond));
  ubthread_mutex_destroy(&(sem->mutex));
  kfree(sem);
}

uint32_t sys_arch_sem_wait(struct sys_sem **s, uint32_t timeout) {
  uint32_t time_needed = 0;
  struct sys_sem *sem;
  LWIP_ASSERT("invalid sem", (s != NULL) && (*s != NULL));
  sem = *s;

  ubthread_mutex_lock(&(sem->mutex));
  while (sem->signaled <= 0) {
    if (timeout > 0) {
      time_needed = cond_wait(&(sem->cond), &(sem->mutex), timeout);

      if (time_needed == SYS_ARCH_TIMEOUT) {
        ubthread_mutex_unlock(&(sem->mutex));
        return SYS_ARCH_TIMEOUT;
      }
      /*      ubthread_mutex_unlock(&(sem->mutex));
       return time_needed; */
    }
    else {
      cond_wait(&(sem->cond), &(sem->mutex), 0);
    }
  }
  sem->signaled--;
  ubthread_mutex_unlock(&(sem->mutex));
  return time_needed;
}

int sys_sem_valid(struct sys_sem **s) {
  struct sys_sem *sem = *s;
  if (sem == 0)
    return 0;
  else
    return 1;
}

void sys_sem_set_invalid(struct sys_sem **s) {
  *s = 0x0;
}

err_t sys_mutex_new(sys_mutex_t *mutex) {
  ubthread_mutex_init(&(mutex->mutex), NULL);
  return ERR_OK;
}

void sys_mutex_free(sys_mutex_t *mutex) {
  ubthread_mutex_destroy(&(mutex->mutex));
}

void sys_mutex_lock(sys_mutex_t *mutex) {
  ubthread_mutex_lock(&(mutex->mutex));
}

void sys_mutex_unlock(sys_mutex_t *mutex) {
  ubthread_mutex_unlock(&(mutex->mutex));
}

err_t sys_mbox_new(struct sys_mbox **mb, int size) {
  struct sys_mbox *mbox = 0x0;
  LWIP_UNUSED_ARG(size);

  mbox = (struct sys_mbox *) kmalloc(sizeof(struct sys_mbox));

  if (mbox == NULL)
    return (ERR_MEM);

  mbox->head = 0;
  mbox->tail = 0;
  mbox->wait_send = 0;
  //mbox->size = size;

  //Pass By Reference It's a Pointer
  //ubthread_mutex_init(&mbox->lock, NULL);

  //Pass By Reference It's a Pointer
  sys_sem_new(&mbox->lock, 1);
  sys_sem_new(&mbox->empty, 0);
  sys_sem_new(&mbox->full, 0);

  //mbox->queue = kmalloc(sizeof(void *) * size);//calloc(size, sizeof(void *));

  //if (!mbox->queue) {
  //  return ERR_MEM;
  //}

  *mb = mbox;
  return (ERR_OK);
}

void sys_mbox_free(struct sys_mbox **mb) {
  if ((mb != NULL) && (*mb != SYS_MBOX_NULL)) {
    struct sys_mbox *mbox = *mb;
    sys_arch_sem_wait(&mbox->lock, 0);

    sys_sem_free_internal(mbox->full);
    sys_sem_free_internal(mbox->empty);
    sys_sem_free_internal(mbox->lock);
    mbox->full = mbox->empty = mbox->lock = NULL;
    kfree(mbox);
  }
  //kfree(mbox->queue);
  //mbox->queue = NULL;
}

void sys_mbox_post(struct sys_mbox **mb, void *msg) {
  uint8_t head;
  struct sys_mbox *mbox;
  LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));
  mbox = *mb;

  sys_arch_sem_wait(&mbox->lock, 0);

  LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_post: mbox %p msg %p\n", (void *)mbox, (void *)msg));

  while ((mbox->tail + 1) >= (mbox->head + SYS_MBOX_SIZE)) {
    mbox->wait_send++;
    sys_sem_signal(&mbox->lock);
    sys_arch_sem_wait(&mbox->empty, 0);
    sys_arch_sem_wait(&mbox->lock, 0);
    mbox->wait_send--;
  }

  mbox->msgs[mbox->tail % SYS_MBOX_SIZE] = msg;

  if (mbox->tail == mbox->head) {
    head = 1;
  }
  else {
    head = 0;
  }

  mbox->tail++;

  if (head) {
    sys_sem_signal(&mbox->full);
  }

  sys_sem_signal(&mbox->lock);
}

err_t sys_mbox_trypost(struct sys_mbox **mb, void *msg) {
  uint8_t head;
  struct sys_mbox *mbox;
  LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));
  mbox = *mb;

  sys_arch_sem_wait(&mbox->lock, 0);

  LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_trypost: mbox %p msg %p\n",
      (void *)mbox, (void *)msg));

  if ((mbox->tail + 1) >= (mbox->head + SYS_MBOX_SIZE)) {
    sys_sem_signal(&mbox->lock);
    return ERR_MEM;
  }

  mbox->msgs[mbox->tail % SYS_MBOX_SIZE] = msg;

  if (mbox->tail == mbox->head) {
    head = 1;
  }
  else {
    head = 0;
  }

  mbox->tail++;

  if (head) {
    sys_sem_signal(&mbox->full);
  }

  sys_sem_signal(&mbox->lock);

  return ERR_OK;
}

uint32_t sys_arch_mbox_fetch(struct sys_mbox **mb, void **msg, uint32_t timeout) {
  uint32_t time_needed = 0x0;
  struct sys_mbox *mbox = 0x0;

  LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));
  mbox = *mb;

  /* The mutex lock is quick so we don't bother with the timeout
   stuff here. */
  sys_arch_sem_wait(&mbox->lock, 0);

  while (mbox->head == mbox->tail) {
    sys_sem_signal(&mbox->lock);

    /* We block while waiting for a mail to arrive in the mailbox. We
     must be prepared to timeout. */
    if (timeout != 0) {
      time_needed = sys_arch_sem_wait(&mbox->full, timeout);

      if (time_needed == SYS_ARCH_TIMEOUT) {
        return SYS_ARCH_TIMEOUT;
      }
    }
    else {
      sys_arch_sem_wait(&mbox->full, 0);
    }

    sys_arch_sem_wait(&mbox->lock, 0);
  }

  if (msg != NULL) {
    LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_fetch: mbox %p msg %p\n", (void *)mbox, *msg));
    *msg = mbox->msgs[mbox->head % SYS_MBOX_SIZE];
  }
  else {
    LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_fetch: mbox %p, null msg\n", (void *)mbox));
  }

  mbox->head++;

  if (mbox->wait_send) {
    sys_sem_signal(&mbox->empty);
  }

  sys_sem_signal(&mbox->lock);

  return time_needed;
}

uint32_t sys_arch_mbox_tryfetch(struct sys_mbox **mb, void **msg) {
  struct sys_mbox *mbox;
  LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));
  mbox = *mb;

  sys_arch_sem_wait(&mbox->lock, 0);

  if (mbox->head == mbox->tail) {
    sys_sem_signal(&mbox->lock);
    return SYS_MBOX_EMPTY;
  }

  if (msg != NULL) {
    LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_tryfetch: mbox %p msg %p\n", (void *)mbox, *msg));
    *msg = mbox->msgs[mbox->head % SYS_MBOX_SIZE];
  }
  else {
    LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_tryfetch: mbox %p, null msg\n", (void *)mbox));
  }

  mbox->head++;

  if (mbox->wait_send) {
    sys_sem_signal(&mbox->empty);
  }

  sys_sem_signal(&mbox->lock);

  return 0;
}

int sys_mbox_valid(struct sys_mbox **mb) {
  struct sys_mbox *mbox = *mb;
  if (mbox == NULL)
    return(0);
  else
    return(1);
}

void sys_mbox_set_invalid(struct sys_mbox **mb) {
  *mb = 0x0;
}

sys_thread_t sys_thread_new(const char *name, void (*thread)(void *arg), void *arg, int stacksize, int prio) {
  //void sys_thread_new(void (*function)(void), void *arg) {
  struct sys_thread *new_thread = 0x0;
  //struct thread_start_param *thread_param;
  prio = 1;
  LWIP_ASSERT("Non-positive prio", prio > 0);
  LWIP_ASSERT("Prio is too big", prio < 20);

  new_thread = kmalloc(sizeof(struct sys_thread));
  memset(new_thread, 0x0, sizeof(struct sys_thread));

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
  if (ubthread_create(&new_thread->ubthread, 0x0, (void *) (thread), arg) != 0x0) {
    kpanic("sys_thread_new: ubthread_create");
  }
  return (new_thread);
}

/* OLD */

struct thread_start_param {
  struct sys_thread *thread;
  void (*function)(void *);
  void *arg;
};

static uint32_t cond_wait(ubthread_cond_t *cond, ubthread_mutex_t *mutex, uint32_t timeout) {
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
  spinLock(&netThreadSpinlock);
  for (st = threads; st != NULL; st = st->next) {
    if (st->ubthread == pt) {
      spinUnlock(&netThreadSpinlock);
      return st;
    }
  }
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


int sys_socket(struct thread *td, struct sys_socket_args *args) {
  int error = 0x0;
  int fd = 0x0;
  struct file *nfp = 0x0;

  error = falloc(td, &nfp, &fd);

  if (error)
    return (error);

  nfp->socket = lwip_socket(args->domain, args->type, args->protocol);
  nfp->fd_type = 2;
  kprintf("socket(%i:%i): 0x%X:0x%X:0x%X", nfp->socket, fd, args->domain, args->type, args->protocol);

  if (nfp->fd == 0x0  && nfp->socket) {
    fdestroy(td, nfp, fd);

    td->td_retval[0] = -1;
    error = -1;
  }
  else {
    td->td_retval[0] = fd;//nfp->fd; //MrOlsen 2018index;
  }

  return (error);
}

int sys_setsockopt(struct thread *td, struct sys_setsockopt_args *args) {
  struct file *fd = 0x0;
  getfd(td, &fd, args->s);

  td->td_retval[0] = lwip_setsockopt(fd->socket, args->level, args->name, args->val, args->valsize);
  kprintf("SSO: %i:%i:%i", args->s, fd->socket, td->td_retval[0]);
  td->td_retval[0] = 0;

  return(0);
}

int sys_select(struct thread *td, struct sys_select_args *args) {
  int error = 0;
  int i     = 0;

  fd_set rfds;
  fd_set wfds;
  fd_set efds;

  FD_ZERO(&rfds);
  FD_ZERO(&wfds);
  FD_ZERO(&efds);

  if (args->in != 0x0) {
    for (i = 0;i < args->nd;i++)  {
      rfds[i] = (struct file *)td->o_files[args->in[0]].socket;
    } 
  }

  if (args->ou != 0x0) {
    rfds = (fd_set *)kmalloc(sizeof(fd_set) * args->nd);
    for (i = 0;i < args->nd;i++)  {
      rfds[i] = (struct file *)td->o_files[args->ou[0]].socket;
    } 
  }

  if (args->ex != 0x0) {
    rfds = (fd_set *)kmalloc(sizeof(fd_set) * args->nd);
    for (i = 0;i < args->nd;i++)  {
      rfds[i] = (struct file *)td->o_files[args->ex[0]].socket;
    } 
  }
  

  if ((td->td_retval[0] = lwip_select(nd,&rfds,&wfds,&efds,args->timeval)) == -1) 
    error = -1;

  return(error);
}

/* END */
