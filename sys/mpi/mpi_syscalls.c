#include <mpi/mpi.h>
#include <sys/sysproto.h>

int sys_mpiCreateMbox(struct thread *td, struct sys_mpiCreateMbox_args *args) {
  td->td_retval[0] = mpi_createMbox(args->name);
  return (0x0);
}
