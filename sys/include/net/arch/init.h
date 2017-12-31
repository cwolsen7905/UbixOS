#ifndef __ARCH_INIT_H__
#define __ARCH_INIT_H__

#define TCPIP_INIT_DONE(arg) sys_sem_signal(*(sys_sem_t *)arg)

#endif
