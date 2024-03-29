#pragma once

#ifndef _STDIO_H
#error "Never use <stdio_printf.h> directly; include <stdio.h> instead."
#endif



/* Write formatted output to STREAM.

   This function is a possible cancellation point and therefore not
   marked with __THROW.  */
int fprintf (FILE *__restrict __stream,
		    const char *__restrict __format, ...);
/* Write formatted output to stdout.

   This function is a possible cancellation point and therefore not
   marked with __THROW.  */
int printf (const char *__restrict __format, ...);
/* Write formatted output to S.  */
int sprintf (char *__restrict __s,
		    const char *__restrict __format, ...);


#include "sprintf.h"

int vfprintf (FILE *__restrict __s, const char *__restrict __format,
		     va_list __arg);
/* Write formatted output to stdout from argument list ARG.

   This function is a possible cancellation point and therefore not
   marked with __THROW.  */
int vprintf (const char *__restrict __format, va_list __arg);
/* Write formatted output to S from argument list ARG.  */

/* Maximum chars of output to write in MAXLEN.  */
int snprintf (char *__restrict __s, size_t __maxlen,
		     const char *__restrict __format, ...);

int vsnprintf (char *__restrict __s, size_t __maxlen,
		      const char *__restrict __format, va_list __arg)
     __attribute__ ((__format__ (__printf__, 3, 0)));

/* Write formatted output to a file descriptor.  */
int vdprintf (int __fd, const char *__restrict __fmt,
		     va_list __arg);
int dprintf (int __fd, const char *__restrict __fmt, ...);


/* Read formatted input from STREAM.

   This function is a possible cancellation point and therefore not
   marked with __THROW.  */
int fscanf (FILE *__restrict __stream,
		   const char *__restrict __format, ...);
/* Read formatted input from stdin.

   This function is a possible cancellation point and therefore not
   marked with __THROW.  */
int scanf (const char *__restrict __format, ...);
/* Read formatted input from S.  */
int sscanf (const char *__restrict __s,
		   const char *__restrict __format, ...);


/* Read formatted input from S into argument list ARG.
   This function is a possible cancellation point and therefore not
   marked with __THROW.  */
int vfscanf (FILE *__restrict __s, const char *__restrict __format,
		    va_list __arg);

/* Read formatted input from stdin into argument list ARG.
   This function is a possible cancellation point and therefore not
   marked with __THROW.  */
int vscanf (const char *__restrict __format, va_list __arg);

/* Read formatted input from S into argument list ARG.  */
int vsscanf (const char *__restrict __s,
		    const char *__restrict __format, va_list __arg);

