#ifdef __FreeBSD__
#undef __GNUC__
#undef __GNUC_MINOR__
#include <sys/types.h>
void foo(__va_list);
#endif
