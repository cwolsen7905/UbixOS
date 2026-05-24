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

/* Include lwIP socket API — provides struct msghdr, lwip_sendmsg, lwip_recvmsg,
 * and all other lwip_* socket function declarations. */
#include <net/sockets.h>

/* Get Definitions For These */
#define ERR_NOT_READY 0
#define ERR_TIMED_OUT 1
#define INFINITE_TIME 0

static struct timeval starttime;
static struct spinLock netThreadSpinlock = SPIN_LOCK_INITIALIZER;
static struct sys_thread *threads = 0x0;

static u_int32_t cond_wait(ubthread_cond_t *cond, ubthread_mutex_t *mutex, u_int32_t timeout);
static void sys_sem_free_internal(struct sys_sem *sem);

/* sys_arch layer initializer */
void sys_init() {
  struct timezone tz;
  gettimeofday(&starttime, &tz);
}

#ifdef _IGNORE
static struct sys_sem *sys_sem_new_internal(u_int8_t count) {
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
err_t sys_sem_new(sys_sem_t **sem, u_int8_t count) {
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

u_int32_t sys_arch_sem_wait(struct sys_sem **s, u_int32_t timeout) {
  u_int32_t time_needed = 0;
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

  memset(mbox, 0, sizeof(struct sys_mbox));

  mbox->head = 0;
  mbox->tail = 0;
  mbox->wait_send = 0;
  //mbox->size = size;

  if (sys_sem_new(&mbox->lock, 1) != ERR_OK) {
    kfree(mbox);
    return (ERR_MEM);
  }
  if (sys_sem_new(&mbox->empty, 0) != ERR_OK) {
    sem_destroy(&mbox->lock);
    kfree(mbox);
    return (ERR_MEM);
  }
  if (sys_sem_new(&mbox->full, 0) != ERR_OK) {
    sem_destroy(&mbox->lock);
    sem_destroy(&mbox->empty);
    kfree(mbox);
    return (ERR_MEM);
  }

  *mb = mbox;

  return (ERR_OK);
}

void sys_mbox_free(struct sys_mbox **mb) {
  if ((mb != NULL) && (*mb != SYS_MBOX_NULL)) {
    struct sys_mbox *mbox = *mb;
    *mb = SYS_MBOX_NULL;  /* prevent new callers from entering */
    sys_arch_sem_wait(&mbox->lock, 0);
    sem_destroy(&mbox->full);
    sem_destroy(&mbox->empty);
    mbox->full = mbox->empty = NULL;
    /* Release the lock before destroying it so any waiter sees a valid
     * signal rather than spinning on freed memory. */
    sys_sem_signal(&mbox->lock);
    sem_destroy(&mbox->lock);
    mbox->lock = NULL;
    kfree(mbox);
  }
}

void sys_mbox_post(struct sys_mbox **mb, void *msg) {
  u_int8_t head;
  struct sys_mbox *mbox;
  LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));
  mbox = *mb;

  sys_arch_sem_wait(&mbox->lock, 0);

  LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_post: mbox %p msg %p\n", (void *)mbox, (void *)msg));

  if ((mbox->tail - mbox->head) >= SYS_MBOX_SIZE) {
    mbox->wait_send++;
    do {
      sys_sem_signal(&mbox->lock);
      sys_arch_sem_wait(&mbox->empty, 0);
      sys_arch_sem_wait(&mbox->lock, 0);
    } while ((mbox->tail - mbox->head) >= SYS_MBOX_SIZE);
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
  u_int8_t head;
  struct sys_mbox *mbox;
  LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));
  mbox = *mb;

  sys_arch_sem_wait(&mbox->lock, 0);

  LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_trypost: mbox %p msg %p\n",
          (void *)mbox, (void *)msg));

  if ((mbox->tail - mbox->head) >= SYS_MBOX_SIZE) {
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

u_int32_t sys_arch_mbox_fetch(struct sys_mbox **mb, void **msg, u_int32_t timeout) {
  u_int32_t time_needed = 0x0;
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

u_int32_t sys_arch_mbox_tryfetch(struct sys_mbox **mb, void **msg) {
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
  if (new_thread == NULL) {
    kprintf("sys_thread_new: out of memory\n");
    return (NULL);
  }
  memset(new_thread, 0x0, sizeof(struct sys_thread));

  if (ubthread_create(&new_thread->ubthread, 0x0, (void*)(thread), arg, name) != 0x0) {
    kfree(new_thread);
    kpanic("sys_thread_new: ubthread_create");
  }

  spinLock(&netThreadSpinlock);
  new_thread->next = threads;
  new_thread->timeouts.next = NULL;
  threads = new_thread;
  spinUnlock(&netThreadSpinlock);

  return (new_thread);
}

/* OLD */

struct thread_start_param {
  struct sys_thread *thread;
  void (*function)(void*);
  void *arg;
};

static u_int32_t cond_wait(ubthread_cond_t *cond, ubthread_mutex_t *mutex, u_int32_t timeout) {
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

u_int32_t sys_now() {
  return (sys_unix_now());
}

int sys_socket(struct thread *td, struct sys_socket_args *args) {
  int error = 0x0;
  int fd = 0x0;
  struct file *nfp = 0x0;

  error = falloc(td, &nfp, &fd);

  if (error)
    return (error);

  /* Strip Linux-only flags musl ORs into type; lwIP only understands the
   * low byte (SOCK_STREAM=1, SOCK_DGRAM=2, SOCK_RAW=3). */
  nfp->socket = lwip_socket(args->domain, args->type & 0xFF, args->protocol);
  nfp->fd_type = 2;

  if (nfp->socket < 0) {
    if (fdestroy(td, nfp, fd) != 0x0)
      kprintf("[%s:%i] fdestroy() failed.", __FILE__, __LINE__);

    td->td_retval[0] = -1;
    error = -1;
  }
  else {
    td->td_retval[0] = fd;
  }

  return (error);
}

int sys_setsockopt(struct thread *td, struct sys_setsockopt_args *args) {
  struct file *fd = 0x0;
  getfd(td, &fd, args->s);
  if (!fd) { td->td_retval[0] = -1; return (-1); }

  td->td_retval[0] = lwip_setsockopt(fd->socket, args->level, args->name, args->val, args->valsize);
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
static void posix_to_lwip_addr(u_int8_t *dst, const u_int8_t *src, int len) {
  u_int16_t family;
  if (len < 2) return;
  memcpy(dst, src, len);
  family = src[0] | ((u_int16_t)src[1] << 8); /* little-endian uint16 family */
  dst[0] = (u_int8_t)len;      /* sin_len */
  dst[1] = (u_int8_t)family;   /* sin_family (low byte = AF_INET=2) */
}

static void lwip_to_posix_addr(u_int8_t *dst, const u_int8_t *src, int len) {
  u_int8_t family;
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
  if (!fd) { td->td_retval[0] = -1; return (-1); }

  /*
   * lwip_sendto posts to tcpip_thread, which has no user mappings.
   * Copy the payload into kernel memory so tcpip_thread can safely
   * access it when it calls pbuf_take().
   */
  if (args->len > 65536) { td->td_retval[0] = -EMSGSIZE; return (-1); }
  kbuf = kmalloc(args->len);
  if (!kbuf) {
    td->td_retval[0] = -1;
    return (-1);
  }
  memcpy(kbuf, args->buf, args->len);

  if (args->to && args->tolen > 0 && args->tolen <= 28) {
    u_int32_t kaddr_storage[7]; /* 28 bytes, 4-byte aligned for IS_SOCK_ADDR_ALIGNED */
    u_int8_t *kaddr = (u_int8_t *)kaddr_storage;
    posix_to_lwip_addr(kaddr, (const u_int8_t *)args->to, args->tolen);
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
  u_int8_t kfrom[28];
  unsigned int kfromlen = sizeof(kfrom);
  void *kbuf;

  getfd(td, &fd, args->s);
  if (!fd) { td->td_retval[0] = -1; return (-1); }

  /*
   * lwip_recvfrom writes received data via tcpip_thread which has no user
   * mappings.  Receive into a kernel buffer, then copy to userland.
   */
  if (args->len > 65536) { td->td_retval[0] = -EMSGSIZE; return (-1); }
  kbuf = kmalloc(args->len);
  if (!kbuf) {
    td->td_retval[0] = -1;
    return (-1);
  }

  if (!args->buf) { kfree(kbuf); td->td_retval[0] = -1; return (-1); }

  if (args->from && args->fromlenaddr) {
    ret = lwip_recvfrom(fd->socket, kbuf, args->len, args->flags,
        (void *)kfrom, &kfromlen);
    if (ret > 0)
      memcpy(args->buf, kbuf, ret);
    if (ret >= 0 && kfromlen > 0) {
      unsigned int outlen = *(unsigned int *)args->fromlenaddr;
      if (outlen > kfromlen) outlen = kfromlen;
      lwip_to_posix_addr((u_int8_t *)args->from, kfrom, outlen);
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

int sys_connect(struct thread *td, struct sys_connect_args *args) {
  struct file *fd = 0x0;
  u_int32_t kaddr_storage[7];
  u_int8_t *kaddr = (u_int8_t *)kaddr_storage;
  int ret;

  getfd(td, &fd, args->s);
  if (!fd) { td->td_retval[0] = -1; return (-1); }

  if (args->name && args->namelen > 0 && args->namelen <= 28) {
    posix_to_lwip_addr(kaddr, (const u_int8_t *)args->name, args->namelen);
    ret = lwip_connect(fd->socket, (void *)kaddr, args->namelen);
  } else {
    ret = lwip_connect(fd->socket, (void *)args->name, args->namelen);
  }
  td->td_retval[0] = ret;
  return (ret < 0 ? -1 : 0);
}

int sys_bind(struct thread *td, struct sys_bind_args *args) {
  struct file *fd = 0x0;
  u_int32_t kaddr_storage[7];
  u_int8_t *kaddr = (u_int8_t *)kaddr_storage;
  int ret;

  getfd(td, &fd, args->s);
  if (!fd) { td->td_retval[0] = -1; return (-1); }

  if (args->name && args->namelen > 0 && args->namelen <= 28) {
    posix_to_lwip_addr(kaddr, (const u_int8_t *)args->name, args->namelen);
    ret = lwip_bind(fd->socket, (void *)kaddr, args->namelen);
  } else {
    ret = lwip_bind(fd->socket, (void *)args->name, args->namelen);
  }
  td->td_retval[0] = ret;
  return (ret < 0 ? -1 : 0);
}

int sys_listen(struct thread *td, struct sys_listen_args *args) {
  struct file *fd = 0x0;
  int ret;

  getfd(td, &fd, args->s);
  if (!fd) { td->td_retval[0] = -1; return (-1); }

  ret = lwip_listen(fd->socket, args->backlog);
  td->td_retval[0] = ret;
  return (ret < 0 ? -1 : 0);
}

int sys_accept(struct thread *td, struct sys_accept_args *args) {
  struct file *fd = 0x0;
  struct file *nfp = 0x0;
  int newfd = 0x0;
  u_int8_t kfrom[28];
  unsigned int kfromlen = sizeof(kfrom);
  int newsock;
  int error;

  getfd(td, &fd, args->s);
  if (!fd) { td->td_retval[0] = -1; return (-1); }

  newsock = lwip_accept(fd->socket,
      args->name ? (void *)kfrom : NULL,
      args->name ? &kfromlen     : NULL);

  if (newsock < 0) { td->td_retval[0] = -1; return (-1); }

  error = falloc(td, &nfp, &newfd);
  if (error) { lwip_close(newsock); td->td_retval[0] = -1; return (error); }

  nfp->socket  = newsock;
  nfp->fd_type = 2;

  if (args->name && args->anamelen) {
    unsigned int outlen;
    if (*args->anamelen < 0) {
      lwip_close(newsock);
      td->td_retval[0] = -EINVAL;
      return (-1);
    }
    outlen = (unsigned int)*args->anamelen;
    if (outlen > kfromlen) outlen = kfromlen;
    lwip_to_posix_addr((u_int8_t *)args->name, kfrom, outlen);
    *args->anamelen = (int)kfromlen;
  }

  td->td_retval[0] = newfd;
  return (0);
}

/*
 * sys_sendmsg — gather iovecs into a kernel buffer and call lwip_sendto.
 * Handles MSG_FASTOPEN by stripping it (lwIP doesn't support it).
 */
int sys_sendmsg(struct thread *td, struct sys_sendmsg_args *args) {
  struct file *fd = NULL;
  const struct msghdr *umsg = args->msg;
  struct iovec *uiov;
  size_t total, off;
  unsigned int i;
  void *kbuf = NULL;
  u_int8_t kaddr[28];
  int flags, ret;

  getfd(td, &fd, args->s);
  if (!fd) { td->td_retval[0] = -1; return (-1); }

  /* strip flags lwIP doesn't understand */
#define MSG_FASTOPEN 0x20000000
#define MSG_NOSIGNAL 0x4000
  flags = args->flags & ~(MSG_FASTOPEN | MSG_NOSIGNAL);

  if (umsg == NULL) { td->td_retval[0] = -1; return (-1); }

  /* compute total payload length across iovecs (overflow-safe) */
  uiov = umsg->msg_iov;
  total = 0;
  for (i = 0; i < (unsigned int)umsg->msg_iovlen; i++) {
    if (total + uiov[i].iov_len < total || total + uiov[i].iov_len > 65536) {
      td->td_retval[0] = -EMSGSIZE;
      return (-1);
    }
    total += uiov[i].iov_len;
  }

  if (total == 0) { td->td_retval[0] = 0; return (0); }

  kbuf = kmalloc(total);
  if (!kbuf) { td->td_retval[0] = -1; return (-1); }

  /* gather iovecs into contiguous kernel buffer */
  off = 0;
  for (i = 0; i < (unsigned int)umsg->msg_iovlen; i++) {
    memcpy((char *)kbuf + off, uiov[i].iov_base, uiov[i].iov_len);
    off += uiov[i].iov_len;
  }

  if (umsg->msg_name && umsg->msg_namelen > 0 &&
      umsg->msg_namelen <= (int)sizeof(kaddr)) {
    posix_to_lwip_addr(kaddr, (const u_int8_t *)umsg->msg_name,
        umsg->msg_namelen);
    ret = lwip_sendto(fd->socket, kbuf, (int)total, flags,
        kaddr, umsg->msg_namelen);
  } else {
    ret = lwip_sendto(fd->socket, kbuf, (int)total, flags, NULL, 0);
  }

  kfree(kbuf);
  td->td_retval[0] = (ret >= 0) ? ret : -1;
  return (ret < 0 ? -1 : 0);
}

/*
 * sys_recvmsg — receive into a kernel buffer then scatter to user iovecs.
 */
int sys_recvmsg(struct thread *td, struct sys_recvmsg_args *args) {
  struct file *fd = NULL;
  struct msghdr *umsg = args->msg;
  struct iovec *uiov;
  size_t total, off, chunk;
  unsigned int i;
  void *kbuf = NULL;
  u_int8_t kfrom[28];
  unsigned int kfromlen = sizeof(kfrom);
  int ret;

  getfd(td, &fd, args->s);
  if (!fd) { td->td_retval[0] = -1; return (-1); }

  if (umsg == NULL) { td->td_retval[0] = -1; return (-1); }

  uiov = umsg->msg_iov;
  total = 0;
  for (i = 0; i < (unsigned int)umsg->msg_iovlen; i++) {
    if (total + uiov[i].iov_len < total || total + uiov[i].iov_len > 65536) {
      td->td_retval[0] = -EMSGSIZE;
      return (-1);
    }
    total += uiov[i].iov_len;
  }

  if (total == 0) { td->td_retval[0] = 0; return (0); }

  kbuf = kmalloc(total);
  if (!kbuf) { td->td_retval[0] = -1; return (-1); }

  ret = lwip_recvfrom(fd->socket, kbuf, total, args->flags,
      umsg->msg_name ? (void *)kfrom : NULL,
      umsg->msg_name ? &kfromlen    : NULL);

  if (ret > 0) {
    /* scatter kernel buffer back to user iovecs */
    off = 0;
    for (i = 0; i < (unsigned int)umsg->msg_iovlen && off < (size_t)ret; i++) {
      chunk = uiov[i].iov_len;
      if (chunk > (size_t)ret - off)
        chunk = (size_t)ret - off;
      memcpy(uiov[i].iov_base, (char *)kbuf + off, chunk);
      off += chunk;
    }
    if (umsg->msg_name && kfromlen > 0) {
      unsigned int outlen = (unsigned int)umsg->msg_namelen;
      if (outlen > kfromlen) outlen = kfromlen;
      lwip_to_posix_addr((u_int8_t *)umsg->msg_name, kfrom, outlen);
      umsg->msg_namelen = (int)kfromlen;
    }
  }

  kfree(kbuf);
  td->td_retval[0] = ret;
  return (ret < 0 ? -1 : 0);
}
