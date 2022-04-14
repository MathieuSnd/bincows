#pragma once


#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>


extern int sprintf(char *str, const char *format, ...);
extern int snprintf(char *str, size_t size, const char *format, ...);
extern int vsprintf(char *str, const char *format, va_list ap);
extern int vsnprintf(char *str, size_t size, const char *format, va_list ap);

extern int snprintf (char *__restrict __s, size_t __maxlen,
		     const char *__restrict __format, ...);
