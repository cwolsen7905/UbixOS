#ifndef __LIBCPP_H
#define __LIBCPP_H

/* Heap allocation */
void *operator new(unsigned size);
void *operator new[](unsigned size);
void  operator delete(void *ptr);
void  operator delete[](void *ptr);
void  operator delete(void *ptr, unsigned size);
void  operator delete[](void *ptr, unsigned size);

/* Placement new/delete */
void *operator new(unsigned, void *ptr);
void *operator new[](unsigned, void *ptr);
void  operator delete(void *, void *);
void  operator delete[](void *, void *);

#endif /* __LIBCPP_H */
