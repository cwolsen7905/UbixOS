#ifndef _BOOTLOG_H
#define _BOOTLOG_H
#include <stdbool.h>
#include <string.h>
#include "_krnl.h"

typedef unsigned KLOGRESULT;

#define K_KLOG_SUCCESS	0x00	/* successfull completion of task, finish log message */
#define K_KLOG_PENDING	0x01	/* task hasn't completed yet, delay logging status */
#define K_KLOG_FAILURE	0x02	/* unsuccessfull completion of task, finish log message */

void klog(char *proc, char *entry, KLOGRESULT result, console_t *vtty0);

#endif /* !defined(_BOOTLOG_H) */
