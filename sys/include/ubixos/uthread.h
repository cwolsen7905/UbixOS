#ifndef _UBIXOS_UTHREAD_H
#define _UBIXOS_UTHREAD_H

#include <sys/cdefs.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct uthread {
  struct uthread *uthread_pointer;
  size_t uthread_size;
  unsigned long uthread_flags;
  void *tls_master_mmap;
  size_t tls_master_size;
  size_t tls_master_align;
  void *tls_mmap;
  size_t stack_size;
  void *arg_mmap;
  size_t arg_size;
  size_t __uthread_reserved[4];
};

#ifdef __cplusplus
}
#endif

#endif
