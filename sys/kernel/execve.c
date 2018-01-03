#include <lib/kprintf.h>
#include <sys/thread.h>
#include <sys/sysproto.h>

int sys_execve( struct thread *td, struct sys_execve_args *args ) {
  int ret = sys_exec( td, args->fname, args->argv, args->envp );
  kprintf("RETURNING: [%i]\n", ret);
  return (ret);
}
