// (C) 2002-2026 The UbixOS Project
// Public interface for the UbixOS minimal C++ ABI library.
#ifndef _UBIX_CXXABI_H
#define _UBIX_CXXABI_H

void *operator new(unsigned size);
void *operator new[](unsigned size);
void  operator delete(void *ptr);
void  operator delete[](void *ptr);
void  operator delete(void *ptr, unsigned size);
void  operator delete[](void *ptr, unsigned size);

void *operator new(unsigned, void *ptr);
void *operator new[](unsigned, void *ptr);
void  operator delete(void *, void *);
void  operator delete[](void *, void *);

#endif /* _UBIX_CXXABI_H */
