// (C) 2002-2026 The UbixOS Project
// Minimal Itanium C++ ABI for UbixOS (single-threaded, no exceptions).
// Provides the compiler-generated call sites that C++ code needs without
// pulling in the full LLVM libc++abi (which requires libc++ internal headers).
// __cxa_atexit and __cxa_finalize are intentionally omitted — musl provides them.

#include <stdlib.h>

extern "C" {

// ---------------------------------------------------------------------------
// Pure / deleted virtual traps
// ---------------------------------------------------------------------------
void __pure_virtual(void)         { abort(); }
void __cxa_pure_virtual(void)     { abort(); }
void __cxa_deleted_virtual(void)  { abort(); }

// ---------------------------------------------------------------------------
// DSO handle — data symbol used as cookie by __cxa_atexit.
// ---------------------------------------------------------------------------
void *__dso_handle = 0;

// ---------------------------------------------------------------------------
// Static-local initialisation guards (Itanium ABI, single-threaded).
// Byte 0 of the guard word is the "done" flag.
// ---------------------------------------------------------------------------
int  __cxa_guard_acquire(int *g) { return !*(volatile char *)g; }
void __cxa_guard_release(int *g) { *(volatile char *)g = 1; }
void __cxa_guard_abort(int *g)   { (void)g; }

} // extern "C"

// ---------------------------------------------------------------------------
// Standard heap allocation operators
// ---------------------------------------------------------------------------
// size_t is the only correct type for operator new (32-bit on i386, 64-bit on
// aarch64); using `unsigned` only happened to work where size_t == unsigned.
void *operator new(size_t size)                { return malloc(size); }
void *operator new[](size_t size)              { return malloc(size); }
void  operator delete(void *ptr)               { free(ptr); }
void  operator delete[](void *ptr)             { free(ptr); }
void  operator delete(void *ptr, size_t)       { free(ptr); }
void  operator delete[](void *ptr, size_t)     { free(ptr); }

// Placement new/delete — no allocation
void *operator new(size_t, void *ptr)          { return ptr; }
void *operator new[](size_t, void *ptr)        { return ptr; }
void  operator delete(void *, void *)          {}
void  operator delete[](void *, void *)        {}
