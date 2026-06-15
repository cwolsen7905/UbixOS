/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 kernel entry (higher-half long-mode bring-up).
 *
 * QEMU's PVH loader enters at _start in 32-bit protected mode at the PHYSICAL
 * load address (0x100000), paging off, %eax = magic, %ebx -> info.  The kernel is
 * LINKED at the higher half (KERNBASE + 1 MB), so the 32-bit stub must reference
 * every symbol by its physical address (sym - KERNBASE) until paging is on.  It
 * builds a 4-level map with BOTH a low identity window (for the 32->64 transition
 * and, for now, physical access) AND the kernel's higher-half mapping, enables
 * PAE + EFER.LME + paging, enters 64-bit mode at a low (identity) label, then
 * jumps to the high VMA where the kernel proper runs.  This frees the entire low
 * canonical half for user space (standard amd64 layout).
 */

	.set KERNBASE, 0xFFFFFFFF80000000

	/*
	 * PVH boot note.  QEMU enters PHYS32_ENTRY in 32-bit protected mode; it must
	 * be the PHYSICAL entry, so _start - KERNBASE (the LMA, ~0x100000).
	 */
	.section .note.Xen, "a", @note
	.align 4
	.long 4    /* namesz = sizeof("Xen\0") */
	.long 4    /* descsz = 4-byte entry address */
	.long 18   /* type = XEN_ELFNOTE_PHYS32_ENTRY */
	.asciz "Xen"
	.align 4
	.long _start - KERNBASE /* 32-bit physical entry point */
	.align 4

	.section .text.boot, "ax"
	.code32
	.globl _start
	.type _start, @function
_start:
	cli
	movl $(_stack_top - KERNBASE), %esp /* physical boot stack */
	movl %eax, %edi                     /* stash multiboot magic -> arg0 (rdi) */
	movl %ebx, %esi                     /* stash multiboot info  -> arg1 (rsi) */

	/* Zero BSS (physical range; covers the page tables below). */
	movl $(__bss_start - KERNBASE), %ecx
	movl $(__bss_end - KERNBASE), %edx
	subl %ecx, %edx
	pushl %edi
	pushl %esi
	movl %ecx, %edi
	movl %edx, %ecx
	xorl %eax, %eax
	rep stosb
	popl %esi
	popl %edi

	/* PML4[0] = pdpt_lo | present|write   (low 1 GB identity, for the transition). */
	movl $(pdpt_lo - KERNBASE), %eax
	orl $0x3, %eax
	movl %eax, (pml4 - KERNBASE)
	/* PML4[511] = pdpt_hi | present|write  (kernel higher half: 0xFFFFFFFF80000000). */
	movl $(pdpt_hi - KERNBASE), %eax
	orl $0x3, %eax
	movl %eax, (pml4 - KERNBASE + 511*8)
	/* PML4[256] = pdpt_pm | present|write  (physmap: 0xFFFF800000000000 -> phys 0). */
	movl $(pdpt_pm - KERNBASE), %eax
	orl $0x3, %eax
	movl %eax, (pml4 - KERNBASE + 256*8)

	/* pdpt_lo[0] = pd | present|write. */
	movl $(pd - KERNBASE), %eax
	orl $0x3, %eax
	movl %eax, (pdpt_lo - KERNBASE)
	/* pdpt_hi[510] = pd | present|write  (0xFFFFFFFF80000000 -> PDPT idx 510; same PD). */
	movl %eax, (pdpt_hi - KERNBASE + 510*8)
	/* pdpt_pm[0] = pd | present|write  (physmap base -> PDPT idx 0; same PD). */
	movl %eax, (pdpt_pm - KERNBASE)

	/* pd[i] = (i*2 MB) | present|write|PS  for i = 0..511  (maps phys 0..1 GB). */
	movl $(pd - KERNBASE), %ebx
	xorl %eax, %eax /* physical address, low dword */
	movl $512, %ecx
1:
	movl %eax, %edx
	orl $0x83, %edx /* present|write|PS(2 MB page) */
	movl %edx, (%ebx)
	movl $0, 4(%ebx) /* phys < 4 GB -> high dword 0 */
	addl $0x200000, %eax
	addl $8, %ebx
	loop 1b

	/* CR4.PAE (bit 5). */
	movl %cr4, %eax
	orl $0x20, %eax
	movl %eax, %cr4

	/* CR3 = PML4 (physical). */
	movl $(pml4 - KERNBASE), %eax
	movl %eax, %cr3

	/* EFER.LME (MSR 0xC0000080, bit 8). */
	movl $0xC0000080, %ecx
	rdmsr
	orl $0x100, %eax
	wrmsr

	/* CR0.PG (bit 31) + PE (bit 0) -> activate long mode. */
	movl %cr0, %eax
	orl $0x80000001, %eax
	movl %eax, %cr0

	/* Load the 64-bit GDT (physical pseudo-descriptor) and far-jump to the low
	 * (identity-reached) 64-bit entry. */
	lgdt (gdt64_ptr - KERNBASE)
	ljmp $0x08, $(long_low - KERNBASE)

	.code64
long_low:
	/* In 64-bit mode now; both the identity and higher-half maps are live.  Jump to
	 * the kernel's real (high VMA) entry. */
	movabs $long_high, %rax
	jmp *%rax

long_high:
	movw $0x10, %ax /* 64-bit data selector */
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	movw %ax, %fs
	movw %ax, %gs
	movabs $_stack_top, %rsp /* higher-half stack */

	/* %edi/%esi still hold the multiboot magic/info (zero-extended). */
	call kmain_x86_64

	/* kmain should not return; park if it does. */
2:
	hlt
	jmp 2b
	.size _start, . - _start

	/* Minimal long-mode GDT: null, 64-bit ring-0 code (L=1), 64-bit ring-0 data.
	 * The base is the PHYSICAL gdt64 (valid via the retained identity map); C-side
	 * x86_64_usermode_init() installs the full higher-half GDT + TSS later. */
	.section .rodata
	.align 16
gdt64:
	.quad 0x0000000000000000 /* null */
	.quad 0x00209A0000000000 /* code: P, DPL0, S, exec/read, L */
	.quad 0x0000920000000000 /* data: P, DPL0, S, read/write */
gdt64_ptr:
	.word gdt64_ptr - gdt64 - 1
	.quad gdt64 - KERNBASE

	/* Page tables (4 KB each).  .bss is zeroed by the stub above. */
	.section .bss
	.align 4096
pml4:
	.skip 4096
pdpt_lo:
	.skip 4096
pdpt_hi:
	.skip 4096
pdpt_pm:
	.skip 4096
pd:
	.skip 4096
