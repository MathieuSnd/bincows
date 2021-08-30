#ifndef SPRINTF_H
#define SPRINTF_H


#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>


int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vsprintf(char *str, const char *format, va_list ap);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);


void set_print_fun(void* fun);
int kprintf(const char* format, ...);

#endif
