extern "C" {
#include <stdlib.h>

/* Pure/deleted virtual call traps */
void __pure_virtual(void)         { while (1); }
void __cxa_pure_virtual(void)     { while (1); }
void __cxa_deleted_virtual(void)  { while (1); }

/*
 * DSO handle — must be a data symbol (void *), not a function.
 * __cxa_atexit receives &__dso_handle as a cookie; a function pointer
 * used as a data address is wrong and breaks DSO-scoped atexit chains.
 */
void *__dso_handle = 0;

/* atexit/finalize stubs — no destructor support in static binaries */
int  __cxa_atexit(void (*)(void *), void *, void *) { return 0; }
void __cxa_finalize(void *)                         {}

/*
 * Static-local initialisation guards — single-threaded implementation.
 * The Itanium C++ ABI stores the "done" flag in byte 0 of the guard word.
 * Return 1 from acquire if initialisation is needed, 0 if already done.
 */
int  __cxa_guard_acquire(int *g) { return !*(volatile char *)g; }
void __cxa_guard_release(int *g) { *(volatile char *)g = 1; }
void __cxa_guard_abort(int *g)   { (void)g; }

} /* extern "C" */

#include <libcpp.h>

/* Standard heap allocation */
void *operator new(unsigned size)              { return malloc(size); }
void *operator new[](unsigned size)            { return malloc(size); }
void  operator delete(void *ptr)               { free(ptr); }
void  operator delete[](void *ptr)             { free(ptr); }
void  operator delete(void *ptr, unsigned)     { free(ptr); }
void  operator delete[](void *ptr, unsigned)   { free(ptr); }

/* Placement new/delete — no allocation, ptr passed straight through */
void *operator new(unsigned, void *ptr)        { return ptr; }
void *operator new[](unsigned, void *ptr)      { return ptr; }
void  operator delete(void *, void *)          {}
void  operator delete[](void *, void *)        {}

