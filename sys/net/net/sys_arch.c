#include <sys/types.h>
#include <string.h>
#include <ubixos/sched.h>
#include <ubixos/ubthread.h>
#include <ubixos/kpanic.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <sys/sysproto_posix.h>
#include <sys/descrip.h>

#include "net/debug.h"
#include "net/sys.h"
#include "net/opt.h"
#include "net/stats.h"
#include <net/arch/sys_arch.h>

#include <ubixos/spinlock.h>
#include <ubixos/sem.h>

/* Forward-declare lwip socket functions to avoid pulling in sockets.h macros. */
int lwip_socket(int domain, int type, int protocol);
int lwip_setsockopt(int s, int level, int optname, const void *optval, int optlen);
int lwip_sendto(int s, const void *dataptr, int size, unsigned int flags, const void *to, int tolen);
int lwip_recvfrom(int s, void *mem, size_t len, int flags, void *from, unsigned int *fromlen);

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

#ifdef _IGNORE
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
#endif

/* Create a new semaphore */
err_t sys_sem_new(sys_sem_t **sem, uint8_t count) {
  return (sem_init(sem, count));

#ifdef __IGNORE
  sys_sem_t *newSem = 0x0;

  if (*sem != 0) {
    kprintf("UH OH!");
  }

  newSem = kmalloc(sizeof(struct sys_sem));
  newSem->signaled = count;

  ubthread_cond_init(&(newSem->cond), NULL);
  ubthread_mutex_init(&(newSem->mutex), NULL);

  *sem = newSem;

  return (ERR_OK);
#endif
}

/* Deallocate semaphore */
void sys_sem_free(struct sys_sem **sem) {
  sem_destroy(sem);

#ifdef _IGNORE
  if ((sem != NULL) && (*sem != SYS_SEM_NULL)) {
    sys_sem_free_internal(*sem);
    *sem = 0x0;
  }
#endif

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

uint32_t sys_arch_sem_wait(struct sys_sem **s, uint32_t timeout) {
  uint32_t time_needed = 0;
  struct sys_sem *sem;
  LWIP_ASSERT("invalid sem", (s != NULL) && (*s != NULL));
  sem = *s;

  ubthread_mutex_lock(&(sem->mutex));
  while (sem->signaled <= 0) {
    if (timeout > 0) {
      /*
       * cond_wait returns 0 on timeout (ETIMEDOUT from the underlying
       * ubthread_cond_timedwait) and non-zero elapsed-ms when woken by
       * a signal.  Map the 0-means-timeout convention to SYS_ARCH_TIMEOUT
       * so that lwIP's timer infrastructure (sys_timeouts_mbox_fetch) works.
       */
      time_needed = cond_wait(&(sem->cond), &(sem->mutex), timeout);
      if (time_needed == 0) {
        ubthread_mutex_unlock(&(sem->mutex));
        return SYS_ARCH_TIMEOUT;
      }
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

  mbox = (struct sys_mbox*) kmalloc(sizeof(struct sys_mbox));

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
    /*
     sys_sem_free_internal(mbox->full);
     sys_sem_free_internal(mbox->empty);
     sys_sem_free_internal(mbox->lock);
     */
    sem_destroy(mbox->full);
    sem_destroy(mbox->empty);
    sem_destroy(mbox->lock);

    mbox->full = mbox->empty = mbox->lock = NULL;
    kfree(mbox);
    *mb = 0x0;
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
    return (0);
  else
    return (1);
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
  if (ubthread_create(&new_thread->ubthread, 0x0, (void*) (thread), arg) != 0x0) {
    kpanic("sys_thread_new: ubthread_create");
  }
  return (new_thread);
}

/* OLD */

struct thread_start_param {
  struct sys_thread *thread;
  void (*function)(void*);
  void *arg;
};

static uint32_t cond_wait(ubthread_cond_t *cond, ubthread_mutex_t *mutex, uint32_t timeout) {
  ubthread_cond_t ubcond = *cond;
  struct timeval rtime1, rtime2, deadline;
  struct timezone tz;
  unsigned int tdiff;

  /*
   * Arm the cond before releasing the mutex.  ubthread_cond_signal /
   * ubthread_cond_broadcast set lock=FALSE; we spin here waiting for
   * that transition.  Without arming first the lock starts FALSE and
   * the spin exits immediately without ever waiting.
   */
  ubcond->lock = TRUE;

  if (timeout > 0) {
    gettimeofday(&rtime1, &tz);
    deadline.tv_sec  = rtime1.tv_sec  + (long)(timeout / 1000);
    deadline.tv_usec = rtime1.tv_usec + (long)((timeout % 1000) * 1000);
    if (deadline.tv_usec >= 1000000L) {
      deadline.tv_sec++;
      deadline.tv_usec -= 1000000L;
    }

    ubthread_mutex_unlock(mutex);

    for (;;) {
      if (ubcond->lock == FALSE)
        break;
      gettimeofday(&rtime2, &tz);
      if (rtime2.tv_sec > deadline.tv_sec ||
          (rtime2.tv_sec == deadline.tv_sec &&
           rtime2.tv_usec >= deadline.tv_usec))
        break;
      sched_yield();
    }

    ubthread_mutex_lock(mutex);

    if (ubcond->lock == TRUE)
      return 0;   /* timed out — caller maps 0 to SYS_ARCH_TIMEOUT */

    gettimeofday(&rtime2, &tz);
    tdiff = (rtime2.tv_sec - rtime1.tv_sec) * 1000 +
            (rtime2.tv_usec - rtime1.tv_usec) / 1000;
    return tdiff > 0 ? tdiff : 1;
  } else {
    ubthread_mutex_unlock(mutex);
    while (ubcond->lock == TRUE)
      sched_yield();
    ubthread_mutex_lock(mutex);
    return 0;
  }
}

static struct sys_thread* current_thread(void) {
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

struct sys_timeouts* sys_arch_timeouts(void) {
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

  if (nfp->fd == 0x0 && nfp->socket) {
    if (fdestroy(td, nfp, fd) != 0x0)
      kprintf("[%s:%i] fdestroy() failed.", __FILE__, __LINE__);

    td->td_retval[0] = -1;
    error = -1;
  }
  else {
    td->td_retval[0] = fd;  //nfp->fd; //MrOlsen 2018index;
  }

  return (error);
}

int sys_setsockopt(struct thread *td, struct sys_setsockopt_args *args) {
  struct file *fd = 0x0;
  getfd(td, &fd, args->s);

  td->td_retval[0] = lwip_setsockopt(fd->socket, args->level, args->name, args->val, args->valsize);
  kprintf("SSO: %i:%i:%i", args->s, fd->socket, td->td_retval[0]);
  td->td_retval[0] = 0;

  return (0);
}

/*
 * Userland (musl/Linux) sockaddr_in has no sin_len byte:
 *   [uint16 family][uint16 port][uint32 addr][char zero[8]]   (16 bytes)
 * lwIP sockaddr_in has a BSD-style sin_len at byte 0:
 *   [uint8 sin_len][uint8 family][uint16 port][uint32 addr][char zero[8]] (16 bytes)
 *
 * posix_to_lwip_addr() copies the userland address into a kernel buffer and
 * fixes the first two bytes so lwIP's IS_SOCK_ADDR_TYPE_VALID check passes.
 *
 * lwip_to_posix_addr() does the reverse for addresses returned by lwIP
 * (e.g. from recvfrom) before writing them back to userland.
 */
static void posix_to_lwip_addr(uint8_t *dst, const uint8_t *src, int len) {
  uint16_t family;
  if (len < 2) return;
  memcpy(dst, src, len);
  family = src[0] | ((uint16_t)src[1] << 8); /* little-endian uint16 family */
  dst[0] = (uint8_t)len;      /* sin_len */
  dst[1] = (uint8_t)family;   /* sin_family (low byte = AF_INET=2) */
}

static void lwip_to_posix_addr(uint8_t *dst, const uint8_t *src, int len) {
  uint8_t family;
  if (len < 2) return;
  memcpy(dst, src, len);
  family = src[1];            /* lwIP sin_family */
  dst[0] = family;            /* posix family low byte */
  dst[1] = 0;                 /* posix family high byte */
}

int sys_sendto(struct thread *td, struct sys_sendto_args *args) {
  struct file *fd = 0x0;
  int ret;
  void *kbuf;

  getfd(td, &fd, args->s);

  /*
   * lwip_sendto posts to tcpip_thread, which has no user mappings.
   * Copy the payload into kernel memory so tcpip_thread can safely
   * access it when it calls pbuf_take().
   */
  kbuf = kmalloc(args->len);
  if (!kbuf) {
    td->td_retval[0] = -1;
    return (-1);
  }
  memcpy(kbuf, args->buf, args->len);

  if (args->to && args->tolen > 0 && args->tolen <= 28) {
    uint32_t kaddr_storage[7]; /* 28 bytes, 4-byte aligned for IS_SOCK_ADDR_ALIGNED */
    uint8_t *kaddr = (uint8_t *)kaddr_storage;
    posix_to_lwip_addr(kaddr, (const uint8_t *)args->to, args->tolen);
    ret = lwip_sendto(fd->socket, kbuf, args->len, args->flags,
        (void *)kaddr, args->tolen);
  } else {
    ret = lwip_sendto(fd->socket, kbuf, args->len, args->flags,
        args->to, args->tolen);
  }
  kfree(kbuf);
  td->td_retval[0] = (ret >= 0) ? ret : -1;
  return (ret < 0 ? -1 : 0);
}

int sys_recvfrom(struct thread *td, struct sys_recvfrom_args *args) {
  struct file *fd = 0x0;
  int ret;
  uint8_t kfrom[28];
  unsigned int kfromlen = sizeof(kfrom);
  void *kbuf;

  getfd(td, &fd, args->s);

  /*
   * lwip_recvfrom writes received data via tcpip_thread which has no user
   * mappings.  Receive into a kernel buffer, then copy to userland.
   */
  kbuf = kmalloc(args->len);
  if (!kbuf) {
    td->td_retval[0] = -1;
    return (-1);
  }

  if (args->from && args->fromlenaddr) {
    ret = lwip_recvfrom(fd->socket, kbuf, args->len, args->flags,
        (void *)kfrom, &kfromlen);
    if (ret > 0)
      memcpy(args->buf, kbuf, ret);
    if (ret >= 0 && kfromlen > 0) {
      unsigned int outlen = *(unsigned int *)args->fromlenaddr;
      if (outlen > kfromlen) outlen = kfromlen;
      lwip_to_posix_addr((uint8_t *)args->from, kfrom, outlen);
      *(unsigned int *)args->fromlenaddr = kfromlen;
    }
  } else {
    ret = lwip_recvfrom(fd->socket, kbuf, args->len, args->flags,
        NULL, NULL);
    if (ret > 0)
      memcpy(args->buf, kbuf, ret);
  }
  kfree(kbuf);
  td->td_retval[0] = ret;
  return (ret < 0 ? -1 : 0);
}
