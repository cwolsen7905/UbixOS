#ifndef _SETJMP_H_
#define _SETJMP_H_

/*
 * i386 jmp_buf: EBX, ESI, EDI, EBP, ESP, EIP
 */
#define _JBLEN 6

typedef int jmp_buf[_JBLEN];

#ifdef __cplusplus
extern "C" {
#endif

int  setjmp(jmp_buf);
void longjmp(jmp_buf, int) __attribute__((__noreturn__));

#ifdef __cplusplus
}
#endif

#endif /* !_SETJMP_H_ */
