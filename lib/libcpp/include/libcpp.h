#ifndef __LIBCPP_H
#define __LIBCPP_H

void * operator new(unsigned size);
void   operator delete(void * ptr);
void * operator new[](unsigned size);
void   operator delete[](void * ptr);

#endif
