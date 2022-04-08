/* Copyright (C) 1991-2021 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

/*
 *	POSIX Standard: 5.1.2 Directory Operations	<dirent.h>
 */

#ifndef	_DIRENT_H
#define	_DIRENT_H	1

#ifdef ISSOU_CA_DOIT_PAS_ETRE_DEFINI_CA

#include "unistd.h"


typedef unsigned long long ino_t;

struct dirent {
    ino_t d_ino;
    off_t d_off;

    unsigned short int d_reclen;
    unsigned char d_type;
    char d_name[256];		/* We must not include limits.h! */
};


typedef struct DIR DIR;


/* File types for `d_type'.  */
enum
  {
    DT_UNKNOWN = 0,
# define DT_UNKNOWN	DT_UNKNOWN
    DT_FIFO = 1,
# define DT_FIFO	DT_FIFO
    DT_CHR = 2,
# define DT_CHR		DT_CHR
    DT_DIR = 4,
# define DT_DIR		DT_DIR
    DT_BLK = 6,
# define DT_BLK		DT_BLK
    DT_REG = 8,
# define DT_REG		DT_REG
    DT_LNK = 10,
# define DT_LNK		DT_LNK
    DT_SOCK = 12,
# define DT_SOCK	DT_SOCK
    DT_WHT = 14
# define DT_WHT		DT_WHT
  };


/* Convert between stat structure types and directory types.  */
// # define IFTODT(mode)	(((mode) & 0170000) >> 12)
// # define DTTOIF(dirtype)	((dirtype) << 12)


/* This is the data type of directory stream objects.
   The actual structure is opaque to users.  */

/* Open a directory stream on NAME.
   Return a DIR stream on the directory, or NULL if it could not be opened.

   This function is a possible cancellation point and therefore not
   marked with __THROW.  */
extern DIR *opendir (const char *__name);

/* Same as opendir, but open the stream on the file descriptor FD.

   This function is a possible cancellation point and therefore not
   marked with __THROW.  */
//extern DIR *fdopendir (int __fd);

/* Close the directory stream DIRP.
   Return 0 if successful, -1 if not.

   This function is a possible cancellation point and therefore not
   marked with __THROW.  */
extern int closedir (DIR *__dirp);

/* Read a directory entry from DIRP.  Return a pointer to a `struct
   dirent' describing the entry, or NULL for EOF or error.  The
   storage returned may be overwritten by a later readdir call on the
   same DIR stream.

   If the Large File Support API is selected we have to use the
   appropriate interface.

   This function is a possible cancellation point and therefore not
   marked with __THROW.  */
extern struct dirent *readdir (DIR *__dirp);


/* Rewind DIRP to the beginning of the directory.  */
extern void rewinddir (DIR *__dirp) __THROW;


/* Seek to position POS on DIRP.  */
extern void seekdir (DIR *__dirp, long int __pos) __THROW;

/* Return the current position of DIRP.  */
extern long int telldir (DIR *__dirp) __THROW;


/* Return the file descriptor used by DIRP.  */
extern int dirfd (DIR *__dirp) __THROW;


/* `MAXNAMLEN' is the BSD name for what POSIX calls `NAME_MAX'.  */
#ifdef NAME_MAX
# define MAXNAMLEN	NAME_MAX
#else
# define MAXNAMLEN	255
#endif

#define __need_size_t
#include <stddef.h>

/* Scan the directory DIR, calling SELECTOR on each directory entry.
   Entries for which SELECT returns nonzero are individually malloc'd,
   sorted using qsort with CMP, and collected in a malloc'd array in
   *NAMELIST.  Returns the number of entries selected, or -1 on error.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
# ifndef __USE_FILE_OFFSET64
extern int scandir (const char *__restrict __dir,
		    struct dirent ***__restrict __namelist,
		    int (*__selector) (const struct dirent *),
		    int (*__cmp) (const struct dirent **,
				  const struct dirent **));

/* Similar to `scandir' but a relative DIR name is interpreted relative
   to the directory for which DFD is a descriptor.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern int scandirat (int __dfd, const char *__restrict __dir,
		      struct dirent ***__restrict __namelist,
		      int (*__selector) (const struct dirent *),
		      int (*__cmp) (const struct dirent **,
				    const struct dirent **))
     __nonnull ((2, 3));


/* Read directory entries from FD into BUF, reading at most NBYTES.
   Reading starts at offset *BASEP, and *BASEP is updated with the new
   position after reading.  Returns the number of bytes read; zero when at
   end of directory; or -1 for errors.  */
extern __ssize_t getdirentries (int __fd, char *__restrict __buf,
				size_t __nbytes,
				__off_t *__restrict __basep)
     __THROW __nonnull ((2, 4));


#endif

#endif /* dirent.h  */
#endif /* dirent.h  */
