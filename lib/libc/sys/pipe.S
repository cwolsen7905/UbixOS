/*
 * pipe(int fds[2]) — calls pipe2 (POSIX slot 0x21E).
 *
 * sys_pipe2_args.fildes is 'int *': the kernel dereferences it to write
 * back the two allocated file descriptors.  Our 'fds' argument is already
 * that pointer, sitting at esp+4 when int $0x80 fires, which is exactly
 * where the kernel reads params[0] (= args->fildes).  No stack surgery
 * required — just invoke the syscall and return.
 */
.text
.globl pipe
.type pipe,@function
pipe:
	movl  $0x21E, %eax       /* SYS_pipe2 */
	int   $0x80
	ret
.size pipe, . - pipe
