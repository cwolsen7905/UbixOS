/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _UBIXOS_SMP_H
#define _UBIXOS_SMP_H

#include <sys/types.h>

#define LAPIC_PHYS 0xFEE00000 /* Local APIC MMIO physical base */

/* Local APIC register offsets (from LAPIC_PHYS), Intel SDM Vol.3 Table 10-1. */
#define LAPIC_ID 0x020         /* Local APIC ID (id in bits 24-31) */
#define LAPIC_EOI 0x0B0        /* End-of-interrupt (write 0)       */
#define LAPIC_SVR 0x0F0        /* Spurious-interrupt vector (bit 8 = LAPIC enable) */
#define LAPIC_ICR_LOW 0x300    /* Interrupt command, low dword     */
#define LAPIC_ICR_HIGH 0x310   /* Interrupt command, high dword (dest apic id in 24-31) */
#define LAPIC_LVT_TIMER 0x320  /* LVT timer entry                 */
#define LAPIC_TIMER_INIT 0x380 /* Timer initial count            */
#define LAPIC_TIMER_CUR 0x390  /* Timer current count            */
#define LAPIC_TIMER_DIV 0x3E0  /* Timer divide configuration     */

/* LVT timer mode bits + the IPI/timer vectors we own (above the 0x20-0x2F PIC
 * range and the 0x80/0x81 syscall gates). */
#define LAPIC_TIMER_PERIODIC 0x20000 /* LVT bit 17: periodic mode */
#define LAPIC_LVT_MASKED 0x10000     /* LVT bit 16: masked        */
#define IPI_RESCHED_VECTOR 0xF0      /* "please reschedule" IPI   */
#define LAPIC_TIMER_VECTOR 0xF1      /* per-CPU LAPIC timer tick   */

/* SMP infrastructure (smp-plan Phase 3).  Additive — these change no behaviour
 * until the AP scheduler-entry integration wires them up. */
void lapic_eoi(void);                                   /* signal end-of-interrupt to the LAPIC */
void lapic_send_ipi(u_int8_t apic_id, u_int8_t vector); /* fixed-delivery IPI to one CPU */
void lapic_timer_init(u_int32_t initial_count);         /* arm this CPU's periodic LAPIC timer */

struct cpuinfo_t
{
	u_int8_t id;
	u_int8_t ok; // 1=Ok, 0=Bad
	u_int8_t apic_id, apic_ver;
	u_int32_t signature; // Family, Model, Stepping
	u_int32_t feature;
	u_int32_t max;
	char brand[49]; // Brand name
	char ident[17];
};

extern struct cpuinfo_t cpuinfo[8];

int smpInit(void);
void cpuidDetect();
u_int8_t cpuInfo();
u_int32_t getEflags();
void setEflags(u_int32_t);
void cpuid(u_int32_t, u_int32_t *);
void apicMagic();

/* madt.c — enumerate CPUs from the ACPI MADT into the MI cpu_enum table
 * (smp-plan Phase 1).  Call after cpuInfo() (BSP APIC id known), before
 * apicMagic().  Discovery only — launches nothing. */
void acpi_enum_cpus(u_int8_t bsp_apic_id);

/*
 * smp-plan Phase 3 — bring application processors into the scheduler.
 *
 * DEFAULT OFF.  The AP scheduler entry (c_ap_boot) and per-CPU idle are complete
 * and boot cleanly, but the i386 kernel still carries uniprocessor assumptions
 * that corrupt under genuine multi-core LOAD: the lazy-FPU owner (_usedMath) is a
 * single global, and there is no cross-CPU TLB shootdown.  Until those are fixed
 * (a v3 task) the released APs are only safe while idle.  Leave this 0 for
 * production (single-core, the proven-stable desktop); set to 1 to experiment
 * with true SMP.  When 0, APs park in a low-power cli;hlt loop (no work, no spin).
 */
#define SMP_ENABLE_APS 0

#endif
