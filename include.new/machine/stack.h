/*-
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 * $FreeBSD: releng/10.2/sys/i386/include/stack.h 174195 2007-12-02 20:40:35Z rwatson $
 */

#ifndef _MACHINE_STACK_H_
#define	_MACHINE_STACK_H_

/*
 * Stack trace.
 */
#define INKERNEL(va)	(((vm_offset_t)(va)) >= USRSTACK && \
	    ((vm_offset_t)(va)) < VM_MAX_KERNEL_ADDRESS)

struct i386_frame {
	struct i386_frame	*f_frame;
	int			 f_retaddr;
	int			 f_arg0;
};

#endif /* !_MACHINE_STACK_H_ */