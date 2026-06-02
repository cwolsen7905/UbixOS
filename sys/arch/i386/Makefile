# (C) 2002-2026 The UbixOS Project

include ../../../Makefile.incl
include ../../Makefile.incl

OBJS = support.o strcpy.o strcmp.o strncmp.o memset.o memcmp.o kpanic.o timer.o spinlock.o i386_exec.o sys_call_posix.o sys_call.o bioscall.o fork.o systemtask.o sched_switch.o context_switch.o cpu.o trap.o bios16code.o vesa.o idt.o io.o ap-boot.o smp.o
# SMP (Phase 0): smp.o + ap-boot.o now compile and link, but smpInit() is not
# yet called from boot — APs are not launched.  See docs/design/smp-plan.md.

.include "${UBIX_MK}/ubix.kern.mk"
