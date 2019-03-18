#ifndef __ARCH_LIB_H__
#define __ARCH_LIB_H__

#ifndef _STRING_H_
#ifndef _STRING_H
int strlen(const char *str);
int strncmp(const char *str1, const char *str2, int len);
void bcopy(const void *src, void *dest, int len);
void bzero(void *data, int n);
#endif /* _STRING_H */
#endif /* _STRING_H_ */

#endif
