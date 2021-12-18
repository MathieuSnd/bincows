#ifndef SPRINTF_H
#define SPRINTF_H


#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>


int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vsprintf(char *str, const char *format, va_list ap);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

int vprintf(const char* format, va_list ap);
int printf(const char* format, ...);

void puts(const char* s);

void set_backend_print_fun(void (*fun)(const char *string, size_t length));

#endif
