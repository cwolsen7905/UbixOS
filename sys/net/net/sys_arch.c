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
#include "net/netif.h"    /* netif_list + ip4 accessors for /proc/lwip */
#include "net/ip4_addr.h" /* ip4addr_ntoa_r */
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

/* lwip-audit: pin a wedged tcpip_thread's location (declared early — sys_mbox_post
 * below references post_blocks).  `g_lwip_mbox_returns` advances only when a fetch
 * actually dequeues a message, so fetches > returns means tcpip is blocked in the
 * mbox wait (a lost wakeup with messages sitting in the ring).  `g_lwip_mbox_post_blocks`
 * counts a sys_mbox_post that had to wait for ring space — tcpip blocking while
 * delivering into a full recv/accept mbox.  Both read via /proc/lwip. */
volatile u_int32_t g_lwip_mbox_returns = 0;
volatile u_int32_t g_lwip_mbox_post_blocks = 0;

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
    g_lwip_mbox_post_blocks++;
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

/* tcpip_thread liveness tick (lwip-audit Phase 0): tcpip_thread's main loop
 * blocks in a mailbox fetch every iteration, so this counter advances whenever
 * the net thread runs.  If it stalls while traffic is arriving, tcpip_thread is
 * being starved (the cooperative-scheduler instability).  Read-only via
 * /proc/lwip; formatted by lwip_stats_format(). */
volatile u_int32_t g_lwip_mbox_fetches = 0;

u_int32_t sys_arch_mbox_fetch(struct sys_mbox **mb, void **msg, u_int32_t timeout) {
  u_int32_t time_needed = 0x0;
  struct sys_mbox *mbox = 0x0;

  LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));
  g_lwip_mbox_fetches++;
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
  g_lwip_mbox_returns++;

  if (mbox->wait_send) {
    sys_sem_signal(&mbox->empty);
  }

  sys_sem_signal(&mbox->lock);

  return time_needed;
}

#if MEMP_STATS
/* Pool names in lwip_stats.memp[] order — built from the same memp_std.h X-macro
 * list memp.c uses for memp_pools[], so the indices line up.  Only `desc` is
 * kept; the num/size args (and their struct sizeofs) are discarded by the
 * preprocessor, so no lwIP type definitions are pulled in. */
static const char *const g_memp_names[] = {
#define LWIP_MEMPOOL(name, num, size, desc) desc,
#include "net/priv/memp_std.h"
};
#endif

/**
 * Snapshot key lwIP counters into a text buffer for /proc/lwip (lwip-audit
 * Phase 0).  Surfaces the tcpip_thread liveness tick, per-protocol error/drop
 * counters, and heap + memp-pool high-water marks — so pool exhaustion (a pool
 * at used==max or err>0) and a stalled net thread become observable instead of
 * silent.  @return bytes written (clamped to bufsz).
 */
int lwip_stats_format(char *buf, int bufsz) {
  int n = 0;
#define LWSNAP(...) do { if (n < bufsz) n += snprintf(buf + n, (size_t)(bufsz - n), __VA_ARGS__); } while (0)
  /* Interfaces first: address/mask/gateway + link state, so /proc/lwip doubles
   * as a quick "what's my IP" check. */
  for (struct netif *nif = netif_list; nif != NULL; nif = nif->next) {
    char ip[16], nm[16], gw[16];
    ip4addr_ntoa_r(netif_ip4_addr(nif), ip, sizeof(ip));
    ip4addr_ntoa_r(netif_ip4_netmask(nif), nm, sizeof(nm));
    ip4addr_ntoa_r(netif_ip4_gw(nif), gw, sizeof(gw));
    LWSNAP("netif %c%c%u: ip=%s mask=%s gw=%s mtu=%u %s%s\n", nif->name[0], nif->name[1], (u_int32_t)nif->num, ip, nm,
           gw, (u_int32_t)nif->mtu, (nif->flags & NETIF_FLAG_UP) ? "up" : "down",
           (nif->flags & NETIF_FLAG_LINK_UP) ? " link-up" : "");
  }
  LWSNAP("tcpip_mbox_fetches: %u\n", g_lwip_mbox_fetches);
  LWSNAP("tcpip_mbox_returns: %u  post_blocks: %u\n", g_lwip_mbox_returns, g_lwip_mbox_post_blocks);
#if LINK_STATS
  LWSNAP("link: recv=%u xmit=%u drop=%u memerr=%u lenerr=%u err=%u\n", (u_int32_t)lwip_stats.link.recv,
         (u_int32_t)lwip_stats.link.xmit, (u_int32_t)lwip_stats.link.drop, (u_int32_t)lwip_stats.link.memerr,
         (u_int32_t)lwip_stats.link.lenerr, (u_int32_t)lwip_stats.link.err);
#endif
#if IP_STATS
  LWSNAP("ip:   recv=%u xmit=%u drop=%u chkerr=%u lenerr=%u\n", (u_int32_t)lwip_stats.ip.recv,
         (u_int32_t)lwip_stats.ip.xmit, (u_int32_t)lwip_stats.ip.drop, (u_int32_t)lwip_stats.ip.chkerr,
         (u_int32_t)lwip_stats.ip.lenerr);
#endif
#if TCP_STATS
  LWSNAP("tcp:  recv=%u xmit=%u drop=%u rterr=%u memerr=%u err=%u\n", (u_int32_t)lwip_stats.tcp.recv,
         (u_int32_t)lwip_stats.tcp.xmit, (u_int32_t)lwip_stats.tcp.drop, (u_int32_t)lwip_stats.tcp.rterr,
         (u_int32_t)lwip_stats.tcp.memerr, (u_int32_t)lwip_stats.tcp.err);
#endif
#if UDP_STATS
  LWSNAP("udp:  recv=%u xmit=%u drop=%u chkerr=%u\n", (u_int32_t)lwip_stats.udp.recv, (u_int32_t)lwip_stats.udp.xmit,
         (u_int32_t)lwip_stats.udp.drop, (u_int32_t)lwip_stats.udp.chkerr);
#endif
#if ICMP_STATS
  LWSNAP("icmp: recv=%u xmit=%u drop=%u lenerr=%u chkerr=%u err=%u\n", (u_int32_t)lwip_stats.icmp.recv,
         (u_int32_t)lwip_stats.icmp.xmit, (u_int32_t)lwip_stats.icmp.drop, (u_int32_t)lwip_stats.icmp.lenerr,
         (u_int32_t)lwip_stats.icmp.chkerr, (u_int32_t)lwip_stats.icmp.err);
#endif
#if MEM_STATS
  LWSNAP("heap: used=%u max=%u avail=%u err=%u\n", (u_int32_t)lwip_stats.mem.used, (u_int32_t)lwip_stats.mem.max,
         (u_int32_t)lwip_stats.mem.avail, (u_int32_t)lwip_stats.mem.err);
#endif
#if MEMP_STATS
  for (int i = 0; i < MEMP_MAX; i++) {
    struct stats_mem *m = lwip_stats.memp[i];
    if (m == NULL || (m->max == 0 && m->used == 0 && m->err == 0))
      continue;
    LWSNAP("pool %-16s used=%u max=%u total=%u err=%u\n", g_memp_names[i], (u_int32_t)m->used, (u_int32_t)m->max,
           (u_int32_t)m->avail, (u_int32_t)m->err);
  }
#endif
#undef LWSNAP
  return n;
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

  /* Run lwIP's threads (tcpip_thread, the virtio RX poll thread) at QOS_KERNEL.
   * They default to QOS_DEFAULT, the same band as user processes — so on the
   * non-preemptible aarch64 scheduler a user process that busy-polls in a syscall
   * (the SSH relay spinning in sys_select, plus tcsh spinning in tty_fo_read) wins
   * every round-robin turn and starves tcpip_thread.  Inbound TCP segments then
   * pile up unprocessed in its mailbox, the socket's rcvevent never fires, and
   * select() never reports the socket readable (SSH input freeze; intermittent
   * ping loss).  ubthread_create stores the new task's pid in ->ubthread. */
  {
    kTask_t *nt = schedFindTask((u_int32_t)(uintptr_t)new_thread->ubthread);
    if (nt != NULL)
      sched_set_priority(nt, QOS_KERNEL);
  }

  return (new_thread);
}

/* OLD */

struct thread_start_param {
  struct sys_thread *thread;
  void (*function)(void*);
  void *arg;
};

/* Scheduler tick rate (PIT_TIMER in <isa/pit.h>); inlined here because that
 * header pulls in device declarations that clash with the lwIP socket headers
 * (duplicate struct iovec). */
#define SYS_ARCH_HZ 200

/* sched_wait_event predicate: the cond is "signaled" once its lock clears
 * (ubthread_cond_signal/broadcast set it FALSE). */
static int sysarch_cond_signaled(void *arg) {
  return ((ubthread_cond_t)arg)->lock == FALSE;
}

/*
 * Block until the condition is signaled or (timeout > 0) the timeout elapses.
 * Sleeps off the run queue via sched_wait_event[_timeout] instead of spinning
 * on sched_yield(), so a waiting network thread no longer starves the rest of
 * the system (the GUI compositor in particular).
 *
 * @return 0 on timeout (caller maps to SYS_ARCH_TIMEOUT); else elapsed ms
 *         (>=1) when signaled.  timeout == 0 means wait forever.
 */
static u_int32_t cond_wait(ubthread_cond_t *cond, ubthread_mutex_t *mutex, u_int32_t timeout) {
  ubthread_cond_t ubcond = *cond;
  struct timeval rtime1, rtime2;
  struct timezone tz;
  unsigned int tdiff;

  /*
   * Arm the cond before releasing the mutex.  ubthread_cond_signal /
   * ubthread_cond_broadcast set lock=FALSE and wake us; arming first closes the
   * lost-wakeup window (sched_wait_event re-checks the predicate under the
   * scheduler lock before actually sleeping).
   */
  ubcond->lock = TRUE;
  gettimeofday(&rtime1, &tz);
  ubthread_mutex_unlock(mutex);

  if (timeout > 0) {
    u_int32_t ticks = (timeout * SYS_ARCH_HZ) / 1000;
    int timedout;

    if (ticks == 0)
      ticks = 1;
    timedout = sched_wait_event_timeout(ubcond, sysarch_cond_signaled, ubcond, ticks);
    ubthread_mutex_lock(mutex);

    if (timedout && ubcond->lock == TRUE)
      return 0; /* timed out — caller maps 0 to SYS_ARCH_TIMEOUT */
  } else {
    /* Block until signaled.  sched_wait_event() is lost-wakeup-safe by
     * construction (it re-tests the predicate with interrupts disabled before
     * sleeping, and the signaler sets the condition then sched_wakeup_chan()s
     * under the same scheduler lock), so no periodic safety re-check is needed.
     * Verified empirically: a 5 s watchdog never fired across heavy httpget
     * load — the only timeouts seen at 50 ms were legitimate >50 ms network
     * round-trips, not missed wakeups. */
    sched_wait_event(ubcond, sysarch_cond_signaled, ubcond);
    ubthread_mutex_lock(mutex);
  }

  gettimeofday(&rtime2, &tz);
  tdiff = (rtime2.tv_sec - rtime1.tv_sec) * 1000 + (rtime2.tv_usec - rtime1.tv_usec) / 1000;
  return tdiff > 0 ? tdiff : 1;
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

/*
 * Socket fileops (path B of the cross-arch file-syscall plan): the read/write/
 * close handlers for FD_TYPE_SOCKET live here, with the lwIP socket code — this
 * file is not linked on architectures without lwIP, so the generic syscall core
 * (sys/posix/vfs_calls.c) dispatches sockets through fp->f_ops and never refers
 * to lwip_* directly.  Each follows the syscall convention: set td_retval[0],
 * return 0 / errno.
 */
/*
 * lwIP socket refcounts.  lwIP keeps sockets in a single global table
 * (MEMP_NUM_NETCONN entries, numbered from LWIP_SOCKET_OFFSET), with no notion
 * of per-process descriptors.  fork() deep-copies the uBixOS struct file so the
 * child aliases the SAME lwIP socket number — so close() in EITHER process must
 * only drop the lwIP socket when the last alias goes away, mirroring the pipe
 * rfdCNT/wfdCNT scheme.  Indexed by lwIP socket number.
 */
#define SOCK_REFC_MAX (LWIP_SOCKET_OFFSET + MEMP_NUM_NETCONN)
static int g_socket_refcount[SOCK_REFC_MAX];

/**
 * Start tracking a freshly-created lwIP socket (socket()/accept()) with one
 * reference.
 */
static void socket_ref_init(int sock) {
  if (sock >= 0 && sock < SOCK_REFC_MAX)
    g_socket_refcount[sock] = 1;
}

/**
 * Add a reference to an lwIP socket inherited across fork.  Called from
 * fork_copy_fdtable() for every FD_TYPE_SOCKET descriptor.
 */
void socket_fork_ref(int sock) {
  if (sock >= 0 && sock < SOCK_REFC_MAX)
    g_socket_refcount[sock]++;
}

static int socket_fo_read(struct file *fp, struct thread *td, void *buf, size_t nbyte) {
  void *kbuf = kmalloc(nbyte);
  if (!kbuf) {
    td->td_retval[0] = -1;
    return (-1);
  }
  int r = lwip_recv(fp->socket, kbuf, nbyte, 0);
  if (r > 0)
    memcpy(buf, kbuf, (size_t)r);
  kfree(kbuf);
  td->td_retval[0] = r;
  return (0);
}

static int socket_fo_write(struct file *fp, struct thread *td, const void *buf, size_t nbyte) {
  void *kbuf = kmalloc(nbyte);
  if (!kbuf) {
    td->td_retval[0] = -1;
    return (-1);
  }
  memcpy(kbuf, buf, nbyte);
  int r = lwip_send(fp->socket, kbuf, nbyte, 0);
  kfree(kbuf);
  td->td_retval[0] = (r >= 0) ? r : -1;
  return (0);
}

static int socket_fo_close(struct file *fp, struct thread *td, int fd) {
  int sock = fp->socket;
  int last = 1;

  /* Only tear down the lwIP netconn when the LAST alias of this socket closes
   * (fork shares the global lwIP socket — see g_socket_refcount).  Otherwise a
   * forking server's parent close() would destroy the child's connection.
   * Untracked sockets (out of range) fall back to the old always-close path. */
  if (sock >= 0 && sock < SOCK_REFC_MAX) {
    if (g_socket_refcount[sock] > 0)
      g_socket_refcount[sock]--;
    last = (g_socket_refcount[sock] <= 0);
  }
  if (last)
    lwip_close(sock);

  if (fdestroy(td, fp, fd) != 0)
    kprintf("sys_close: fdestroy failed for socket fd %d\n", fd);
  td->td_retval[0] = 0;
  return (0);
}

static struct fileOps socket_ops = {
  .read = socket_fo_read,
  .write = socket_fo_write,
  .close = socket_fo_close,
};

int sys_socket(struct thread *td, struct sys_socket_args *args) {
  int error = 0x0;
  int fd = 0x0;
  struct file *nfp = 0x0;

  error = falloc(td, &nfp, &fd);

  if (error)
    return (error);

  /* uBixOS is IP-only (lwIP); there is no AF_UNIX/AF_LOCAL.  Reject other
   * families with EAFNOSUPPORT specifically — NOT a generic error — because
   * musl's __nscd_query() probes with socket(AF_UNIX) and treats EAFNOSUPPORT as
   * "no nscd, fall through to /etc/group".  Any other errno makes getgrouplist()
   * (hence initgroups()) fail, which aborts an ssh login with the misleading
   * "Error changing user group". */
  if (args->domain != AF_INET && args->domain != AF_INET6) {
    fdestroy(td, nfp, fd);
    td->td_retval[0] = -EAFNOSUPPORT;
    return (EAFNOSUPPORT);
  }

  /* Strip Linux-only flags musl ORs into type; lwIP only understands the
   * low byte (SOCK_STREAM=1, SOCK_DGRAM=2, SOCK_RAW=3). */
  nfp->socket = lwip_socket(args->domain, args->type & 0xFF, args->protocol);
  nfp->fd_type = 2;
  nfp->f_ops = &socket_ops;

  if (nfp->socket < 0) {
    if (fdestroy(td, nfp, fd) != 0x0)
      kprintf("[%s:%i] fdestroy() failed.", __FILE__, __LINE__);

    td->td_retval[0] = -1;
    error = -1;
  }
  else {
    socket_ref_init(nfp->socket);
    td->td_retval[0] = fd;
  }

  return (error);
}

int sys_setsockopt(struct thread *td, struct sys_setsockopt_args *args) {
  struct file *fd = 0x0;
  void *kval = 0x0;
  getfd(td, &fd, args->s);
  if (!fd) { td->td_retval[0] = -1; return (-1); }

  /* Copy the user optval into a kernel buffer before handing it to lwIP.  With
   * LWIP_TCPIP_CORE_LOCKING off, lwip_setsockopt posts optval to tcpip_thread,
   * which runs in a context with NO user mappings — dereferencing the raw user
   * pointer there faults (a wild user address such as 0x18003fa1c).  This is the
   * same kernel-buffer pattern sys_connect/sys_read/sys_write already use. */
  if (args->val != 0x0 && args->valsize > 0) {
    kval = kmalloc(args->valsize);
    if (kval == 0x0) { td->td_retval[0] = -1; return (-1); }
    memcpy(kval, args->val, args->valsize);
  }

  td->td_retval[0] = lwip_setsockopt(fd->socket, args->level, args->name, kval != 0x0 ? kval : args->val, args->valsize);
  td->td_retval[0] = 0;

  if (kval != 0x0)
    kfree(kval);
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
/*
 * Convert + VALIDATE a userland sockaddr before it reaches lwIP.  lwIP's
 * IS_SOCK_ADDR_* checks (in lwip_connect/bind/...) are LWIP_ERROR assertions
 * that PANIC the kernel on a malformed address — so a bad sockaddr from
 * userland must be rejected here, never passed through.  uBixOS speaks IPv4
 * over lwIP today; AF_UNSPEC is allowed (lwip_connect treats it as
 * "disconnect"), anything else (including IPv6, whose FreeBSD-ABI family number
 * 28 differs from lwIP's 10) returns an error so the caller falls back instead
 * of crashing the kernel.  dst must be >= 28 bytes and 4-byte aligned.
 *
 * @return 0 on success, -errno on a malformed/unsupported address.
 */
static int posix_to_lwip_addr(u_int8_t *dst, const u_int8_t *src, int len) {
  u_int16_t family;
  if (!src || len < 2 || len > 28) return (-EINVAL);
  memcpy(dst, src, len);
  family = src[0] | ((u_int16_t)src[1] << 8); /* little-endian uint16 family */
  if (family == AF_INET) {
    if (len < (int)sizeof(struct sockaddr_in)) return (-EINVAL);
  } else if (family != AF_UNSPEC) {
    return (-EAFNOSUPPORT);
  }
  dst[0] = (u_int8_t)len;      /* sin_len */
  dst[1] = (u_int8_t)family;   /* sin_family (low byte = AF_INET=2) */
  return (0);
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

  if (args->to) {
    u_int32_t kaddr_storage[7]; /* 28 bytes, 4-byte aligned for IS_SOCK_ADDR_ALIGNED */
    u_int8_t *kaddr = (u_int8_t *)kaddr_storage;
    int aerr = posix_to_lwip_addr(kaddr, (const u_int8_t *)args->to, args->tolen);
    if (aerr != 0) { kfree(kbuf); td->td_retval[0] = aerr; return (-1); }
    ret = lwip_sendto(fd->socket, kbuf, args->len, args->flags,
        (void *)kaddr, args->tolen);
  } else {
    /* No destination (connected socket): pass NULL through. */
    ret = lwip_sendto(fd->socket, kbuf, args->len, args->flags, NULL, 0);
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

  ret = posix_to_lwip_addr(kaddr, (const u_int8_t *)args->name, args->namelen);
  if (ret != 0) {
    td->td_retval[0] = ret;
    return (-1);
  }
  ret = lwip_connect(fd->socket, (void *)kaddr, args->namelen);
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

  ret = posix_to_lwip_addr(kaddr, (const u_int8_t *)args->name, args->namelen);
  if (ret != 0) {
    td->td_retval[0] = ret;
    return (-1);
  }
  ret = lwip_bind(fd->socket, (void *)kaddr, args->namelen);
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
  nfp->f_ops   = &socket_ops;
  socket_ref_init(newsock);

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
 * accept4(s, name, anamelen, flags) — the modern accept().  musl's accept()
 * wrapper calls accept4 (FreeBSD slot 541), so without this a server (e.g.
 * dropbear) busy-loops on ENOSYS and never accepts a connection.  Delegates to
 * sys_accept, then applies SOCK_NONBLOCK to the new socket if requested.
 */
int sys_accept4(struct thread *td, struct sys_accept4_args *args) {
  struct sys_accept_args aargs;
  int r;

  aargs.s = args->s;
  aargs.name = args->name;
  aargs.anamelen = args->anamelen;
  r = sys_accept(td, &aargs);

  /* SOCK_NONBLOCK (FreeBSD ABI = 0x20000000): set the accepted fd non-blocking.
   * SOCK_CLOEXEC is ignored (no exec-time fd flags on uBixOS yet). */
  if (r == 0 && (args->flags & 0x20000000)) {
    struct file *nf = 0x0;
    getfd(td, &nf, (int)td->td_retval[0]);
    if (nf)
      lwip_fcntl(nf->socket, F_SETFL, lwip_fcntl(nf->socket, F_GETFL, 0) | O_NONBLOCK);
  }
  return (r);
}

/*
 * recv(s, buf, len, flags) — FreeBSD's obsolete-numbered recv (slot 102), which
 * musl emits.  Equivalent to recvfrom() with no source address.
 */
int sys_orecv(struct thread *td, struct sys_orecv_args *args) {
  struct file *fd = 0x0;
  int ret;
  void *kbuf;

  getfd(td, &fd, args->s);
  if (!fd) { td->td_retval[0] = -1; return (-1); }
  if (args->len > 65536) { td->td_retval[0] = -EMSGSIZE; return (-1); }
  if (!args->buf) { td->td_retval[0] = -1; return (-1); }
  kbuf = kmalloc(args->len);
  if (!kbuf) { td->td_retval[0] = -1; return (-1); }

  ret = lwip_recv(fd->socket, kbuf, args->len, args->flags);
  if (ret > 0)
    memcpy(args->buf, kbuf, ret);
  kfree(kbuf);
  td->td_retval[0] = ret;
  return (ret < 0 ? -1 : 0);
}

/*
 * getsockname(fdes, asa, alen) — the local address of a socket.  Servers call
 * it to log the bound address; lwIP returns a BSD-style sockaddr which
 * lwip_to_posix_addr() converts back to the userland (no-sin_len) form.
 */
int sys_getsockname(struct thread *td, struct sys_getsockname_args *args) {
  struct file *fd = 0x0;
  u_int8_t kfrom[28];
  unsigned int kfromlen = sizeof(kfrom);
  int ret;

  getfd(td, &fd, args->fdes);
  if (!fd) { td->td_retval[0] = -1; return (-1); }

  ret = lwip_getsockname(fd->socket, (void *)kfrom, &kfromlen);
  if (ret < 0) { td->td_retval[0] = -1; return (-1); }

  if (args->asa && args->alen) {
    unsigned int outlen = (unsigned int)*args->alen;
    if (outlen > kfromlen) outlen = kfromlen;
    lwip_to_posix_addr((u_int8_t *)args->asa, kfrom, outlen);
    *args->alen = (int)kfromlen;
  }
  td->td_retval[0] = 0;
  return (0);
}

/*
 * getpeername(fdes, asa, alen) — the remote address of a connected socket.
 */
int sys_getpeername(struct thread *td, struct sys_getpeername_args *args) {
  struct file *fd = 0x0;
  u_int8_t kfrom[28];
  unsigned int kfromlen = sizeof(kfrom);
  int ret;

  getfd(td, &fd, args->fdes);
  if (!fd) { td->td_retval[0] = -1; return (-1); }

  ret = lwip_getpeername(fd->socket, (void *)kfrom, &kfromlen);
  if (ret < 0) { td->td_retval[0] = -1; return (-1); }

  if (args->asa && args->alen) {
    unsigned int outlen = (unsigned int)*args->alen;
    if (outlen > kfromlen) outlen = kfromlen;
    lwip_to_posix_addr((u_int8_t *)args->asa, kfrom, outlen);
    *args->alen = (int)kfromlen;
  }
  td->td_retval[0] = 0;
  return (0);
}

/*
 * getsockopt(s, level, name, val, avalsize) — counterpart to setsockopt; passed
 * straight to lwIP (the level/optname numbers match the lwIP sockets layer the
 * setsockopt path already uses).
 */
int sys_getsockopt(struct thread *td, struct sys_getsockopt_args *args) {
  struct file *fd = 0x0;
  u_int8_t kval[128];
  unsigned int kvalsize;
  int ret;

  getfd(td, &fd, args->s);
  if (!fd) { td->td_retval[0] = -1; return (-1); }
  if (!args->val || !args->avalsize) { td->td_retval[0] = -EINVAL; return (-1); }

  kvalsize = (unsigned int)*args->avalsize;
  if (kvalsize > sizeof(kval)) kvalsize = sizeof(kval);

  ret = lwip_getsockopt(fd->socket, args->level, args->name, kval, &kvalsize);
  if (ret < 0) { td->td_retval[0] = -1; return (-1); }

  memcpy(args->val, kval, kvalsize);
  *args->avalsize = (int)kvalsize;
  td->td_retval[0] = 0;
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

  if (umsg->msg_name) {
    int aerr = posix_to_lwip_addr(kaddr, (const u_int8_t *)umsg->msg_name,
        umsg->msg_namelen);
    if (aerr != 0) { kfree(kbuf); td->td_retval[0] = aerr; return (-1); }
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
