/*
 * Minimal machine/asm.h stub for UbixOS userland i386 build.
 * Provides only the macros actually used by the FreeBSD-derived CSU sources.
 */

#ifndef _MACHINE_ASM_H_
#define _MACHINE_ASM_H_

#ifndef __FBSDID
#define __FBSDID(s)
#endif

#define ENTRY(name)   .globl name; .type name,@function; name:
#define END(name)     .size name, . - name

#endif /* _MACHINE_ASM_H_ */
