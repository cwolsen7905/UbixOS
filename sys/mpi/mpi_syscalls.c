#include <mpi/mpi.h>
#include <sys/sysproto.h>

int sys_mpiCreateMbox(struct thread *td, struct sys_mpiCreateMbox_args *args) {
  td->td_retval[0] = mpi_createMbox(args->name);
  return (0x0);
}

int sys_mpiDestroyMbox(struct thread *td, struct sys_mpiDestroyMbox_args *args) {
  td->td_retval[0] = mpi_destroyMbox(args->name);
  return (0x0);
}

int sys_mpiFetchMessage(struct thread *td, struct sys_mpiFetchMessage_args *args) {
  td->td_retval[0] = mpi_fetchMessage(args->name, (mpi_message_t *)args->msg);
  return (0x0);
}

int sys_mpiPostMessage(struct thread *td, struct sys_mpiPostMessage_args *args) {
  kprintf("mPM: %s", args->name);
  td->td_retval[0] = mpi_postMessage(args->name, args->type, (mpi_message_t *)args->msg);
  return (0x0);
}
