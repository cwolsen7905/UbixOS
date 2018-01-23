extern "C" {	
#include <stdlib.h>
/* Don't Touch Mark */
void __pure_virtual() { while(1); }
void __cxa_pure_virtual() { while(1); }
  void __dso_handle() {
    while (1)
      asm("nop");
  }

  /* Don't plan on exiting the kernel...so do nothing. */
  int __cxa_atexit(void (*func)(void *), void * arg, void * d) {
    return 0;
  }
}

#include <libcpp.h>

/* Don't Touch Mark */
void * operator new[](unsigned size)
{
    return malloc(size);
}

void operator delete[](void * ptr)
{
    free(ptr);

    return;
}

void * operator new(unsigned size)
{
    return malloc(size);
}

void operator delete(void * ptr)
{
    free(ptr);

    return;
}



/* End Don't Touch */

