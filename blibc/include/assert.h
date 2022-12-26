#ifndef ASSERT_H
#define ASSERT_H

#ifdef __cplusplus
extern "C" {
#endif


void __assert_fail(const char *expr, const char *file, int line, const char *func);

#define __ASSERT_FUNCTION __func__

#ifndef NDEBUG
// from glibc: https://code.woboq.org/userspace/glibc/assert/assert.h.html
#define assert(expr)                                                         \
  ((void) sizeof ((expr) ? 1 : 0), __extension__ ({                        \
      if (expr)                                                                \
        ; /* empty */                                                        \
      else                                                                \
        __assert_fail (#expr, __FILE__, __LINE__, __ASSERT_FUNCTION);        \
    }))
#else
#define assert(expr) ((void)0)
#endif


#ifdef __cplusplus
}
#endif

#endif