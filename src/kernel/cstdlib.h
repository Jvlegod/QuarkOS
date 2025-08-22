#ifndef __CSTDLIB_H__
#define __CSTDLIB_H__
#include "ktypes.h"
#include <stdarg.h>

#ifndef STATIC_ASSERT
  #define __SA_CAT_(a,b) a##b
  #define __SA_CAT(a,b)  __SA_CAT_(a,b)
  #define STATIC_ASSERT(cond, msg) \
      typedef char __SA_CAT(static_assert_at_line_, __LINE__)[ (cond) ? 1 : -1 ]
#endif

int strcmp(const char *s1, const char *s2);
size_t strlen(const char *s);
int strncmp(const char *s1, const char *s2, size_t n);
int memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void memcpy(void *dest, const void *src, size_t n);

#endif // __CSTDLIB_H__