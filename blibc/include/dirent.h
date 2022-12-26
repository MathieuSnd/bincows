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

#ifdef __cplusplus
extern "C" {
#endif



#include "unistd.h"

#include "stddef.h"
#include "sys/types.h"


struct dirent {
   ino_t         ino;       /* Inode number */
   size_t        d_reclen;  /* Length of this record */
   unsigned char d_type;    /* Type of file; not supported
                                by all filesystem types */

   uint8_t reserved[7];

    char d_name[256];   /* Null-terminated filename */
};


typedef struct DIR DIR;


/* File types for `d_type'.  */
// unknown
#define DT_UNKNOWN	0

// directory 
#define DT_DIR	3

// block device
#define DT_BLK	4

// regular file
#define DT_REG	5

// syb link
#define DT_LNK	6

// socket
#define DT_SOCK	7


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
int closedir (DIR *__dirp);

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
void rewinddir (DIR *__dirp);


/* Seek to position POS on DIRP.  */
void seekdir (DIR *__dirp, long int __pos);

/* Return the current position of DIRP.  */
extern long int telldir (DIR *__dirp);


/* Return the file descriptor used by DIRP.  */
int dirfd (DIR *__dirp);



/* `MAXNAMLEN' is the BSD name for what POSIX calls `NAME_MAX'.  */
#ifdef NAME_MAX
# define MAXNAMLEN	NAME_MAX
#else
# define MAXNAMLEN	255
#endif


/* Scan the directory DIR, calling SELECTOR on each directory entry.
   Entries for which SELECT returns nonzero are individually malloc'd,
   sorted using qsort with CMP, and collected in a malloc'd array in
   *NAMELIST.  Returns the number of entries selected, or -1 on error.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
/*

POSIX 2008

int scandir (const char *__restrict __dir,
		    struct dirent ***__restrict __namelist,
		    int (*__selector) (const struct dirent *),
		    int (*__cmp) (const struct dirent **,
				  const struct dirent **));
*/

/* Read directory entries from FD into BUF, reading at most NBYTES.
   Reading starts at offset *BASEP, and *BASEP is updated with the new
   position after reading.  Returns the number of bytes read; zero when at
   end of directory; or -1 for errors.  */

/*

POSIX.1
extern __ssize_t getdirentries (int __fd, char *__restrict __buf,
				size_t __nbytes,
				__off_t *__restrict __basep);

*/


#ifdef __cplusplus
}
#endif

#endif /* dirent.h  */
