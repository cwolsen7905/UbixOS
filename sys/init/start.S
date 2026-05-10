/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
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

/* FreeBSD bootinfo offsets */
#define BI_VERSION    0x0
#define BI_KERNELNAME 0x4
#define BI_ENDCOMMON  12
#define BI_SIZE       48
#define BOOTINFO_SIZE 135
#define RB_BOOTINFO   0x80000000  /* have `struct bootinfo *' arg */

/* Multiboot 1 constants */
#define MB_MAGIC      0x1BADB002
#define MB_FLAGS      0x00000003  /* bit0: align modules, bit1: provide memory map */
#define MB_CHECKSUM   -(MB_MAGIC + MB_FLAGS)
#define MB_LOADER_MAGIC 0x2BADB002


/* -------------------------------------------------------------------------
 * Multiboot 1 header + kernel entry point in .text.boot
 *
 * The linker script places .text.boot before all other .text sections so
 * the multiboot magic lands within the first 8 KB of the ELF file, where
 * GRUB scans for it.  Using a named section avoids any dependency on
 * object-file link order.
 * ------------------------------------------------------------------------- */
.section .text.boot, "ax"
.align 4
multiboot_header:
  .long MB_MAGIC
  .long MB_FLAGS
  .long MB_CHECKSUM

/* -------------------------------------------------------------------------
 * Kernel entry point
 * ------------------------------------------------------------------------- */
.globl _start
.code32
_start:
  /*
   * Detect boot protocol.  GRUB (multiboot) always leaves 0x2BADB002 in
   * %eax and a pointer to the multiboot_info struct in %ebx.  The FreeBSD
   * bootloader does not set %eax to this value.
   */
  cmpl    $MB_LOADER_MAGIC, %eax
  je      multiboot_entry

  /* ------------------------------------------------------------------
   * FreeBSD bootloader path (original, unchanged)
   * ------------------------------------------------------------------ */
  movw    $0x1234, 0x472
  pushl   %ebp
  movl    %esp, %ebp
  pushl   $0x00000002
  popfl

  /* Clear the BSS */
  movl    $(_end), %ecx
  movl    $(__bss_start), %edi
  subl    %edi, %ecx
  xorl    %eax, %eax
  cld
  rep
  stosb

  /* Fix Up GS/FS */
  mov     %dx, %ax
  mov     %ax, %fs
  mov     %ax, %gs

  /* Get Boot Args */
  call    get_bootargs

  jmp     load_gdt


  /* ------------------------------------------------------------------
   * Multiboot path
   *
   * On entry: %eax = 0x2BADB002, %ebx = *multiboot_info
   * GRUB provides a valid flat protected-mode environment but does NOT
   * guarantee a usable stack, so we set one up immediately.
   * ------------------------------------------------------------------ */
multiboot_entry:
  /*
   * Preserve the multiboot_info pointer in %esi across the BSS clear
   * (we cannot write to globals until BSS is zeroed).
   */
  movl    %ebx, %esi
  movl    $(kStack + 0x1000), %esp
  xorl    %ebp, %ebp

  /* Clear the BSS */
  movl    $(_end), %ecx
  movl    $(__bss_start), %edi
  subl    %edi, %ecx
  xorl    %eax, %eax
  cld
  rep
  stosb

  /* BSS is now zero — safe to write globals */
  movl    %esi, (_multiboot_info)


  /* ------------------------------------------------------------------
   * Common: load GDT and jump to C
   * Both boot paths converge here.
   * ------------------------------------------------------------------ */
load_gdt:
  lgdtl   (loadGDT)
  mov     $0x10, %eax
  mov     %eax, %ds
  mov     %eax, %es
  mov     %eax, %fs
  mov     %eax, %gs
  mov     %eax, %ss
  mov     $kStack, %eax
  addl    $0x1000, %eax
  mov     %esp, %edx
  mov     %eax, %esp
  mov     %eax, %ebp
  mov     $0x18, %ax
  lldt    %ax
  mov     $0x20, %ax
  ltr     %ax
  ljmp    $0x08, $start_next
start_next:
  call    vmm_init
  pushl   %esp
  mov     %esp, %ebp
  mov     $0xFFFFFFFF, %eax
  mov     %eax, %esp
  pushl   $0xDEAD   /* Stack sentinel */
  pushl   $0xBEEF   /* Boot marker   */
  subl    $0xE, %esp
  call    kmain
  hlt               /* Should never reach here */


/* -------------------------------------------------------------------------
 * FreeBSD bootinfo parser (used only on the FreeBSD boot path)
 * ------------------------------------------------------------------------- */
get_bootargs:
  /*
   * The old style disk boot blocks fake a frame on the stack and
   * did an lret to get here.  The frame on the stack has a return
   * address of 0.
   */
  cmpl    $0, 4(%ebp)
  je      old_boot

  /*
   * We have some form of return address, so this is either the
   * old diskless netboot code, or the new uniform code.  That can
   * be detected by looking at the 5th argument; if it is 0 we are
   * being booted by the new uniform boot code.
   */
  cmpl    $0, 24(%ebp)
  je      new_boot

  hlt

new_boot:
  movl    28(%ebp), %ebx          /* &bootinfo.version */
  movl    BI_VERSION(%ebx), %eax
  cmpl    $1, %eax                /* We only understand version 1 */
  je      1f
  movl    $1, %eax                /* Return status */
  leave
  hlt
  ret
1:
  /* If we have a kernelname, copy it in */
  movl    BI_KERNELNAME(%ebx), %esi
  cmpl    $0, %esi
  je      2f                      /* No kernelname */
  movl    512, %ecx               /* Brute force!!! */
  movl    $_kernelname, %edi
  cmpb    $'/', (%esi)            /* Make sure it starts with a slash */
  je      1f
  movb    $'/', (%edi)
  incl    %edi
  decl    %ecx
1:
  cld
  rep
  movsb

2:
  /*
   * Determine the size of the boot loader's copy of the bootinfo struct.
   * This is impossible to do properly because old versions of the struct
   * don't contain a size field and there are 2 old versions with the same
   * version number.
   */
  movl    $BI_ENDCOMMON, %ecx     /* prepare for sizeless version */
  testl   $RB_BOOTINFO, 8(%ebp)  /* bi_size (and bootinfo) valid? */
  je      got_bi_size             /* no, sizeless version */
  movl    BI_SIZE(%ebx), %ecx
got_bi_size:
  /* Copy the common part of the bootinfo struct */
  movl    %ebx, %esi
  movl    $_bootinfo, %edi
  cmpl    $BOOTINFO_SIZE, %ecx
  jbe     got_common_bi_size
  movl    $BOOTINFO_SIZE, %ecx
got_common_bi_size:
  cld
  rep
  movsb

old_boot:
  movl    8(%ebp), %eax
  movl    %eax, (_boothowto)
  movl    12(%ebp), %eax
  movl    %eax, (_bootdev)
  ret


/* -------------------------------------------------------------------------
 * Data
 * ------------------------------------------------------------------------- */
.data

/* Pointer to the multiboot_info struct passed by GRUB. Zero when booted
 * via the FreeBSD loader.  Readable from C as: extern uint32_t _multiboot_info; */
.globl _multiboot_info
_multiboot_info:
  .long 0

.comm kStack, 0x1000
