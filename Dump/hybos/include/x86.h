#ifndef __TL_X86_H
#define	__TL_X86_H

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * 16 bit MSB: segment, 16 bit LSB: offset
 */
//typedef unsigned farptr;

unsigned inportb(unsigned short port);
void outportb(unsigned port, unsigned val);
unsigned inportw(unsigned short port);
void outportw(unsigned port, unsigned val);
unsigned disable(void);
void enable(void);

#ifdef __cplusplus
}
#endif

#endif

