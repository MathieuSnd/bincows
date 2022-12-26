#ifndef	_UNISTD_H
#define	_UNISTD_H	1

#include <stdint.h>
#include <stddef.h>
#include "sys/types.h"

/* Standard file descriptors.  */
#define	STDIN_FILENO	0	/* Standard input.  */
#define	STDOUT_FILENO	1	/* Standard output.  */
#define	STDERR_FILENO	2	/* Standard error output.  */

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>

// not implemented yet.....
#if 0


/* Schedule an alarm.  In SECONDS seconds, the process will get a SIGALRM.
   If SECONDS is zero, any currently scheduled alarm will be cancelled.
   The function returns the number of seconds remaining until the last
   alarm scheduled would have signaled, or zero if there wasn't one.
   There is no return value to indicate an error, but you can set `errno'
   to 0 and check its value after calling `alarm', and this might tell you.
   The signal may come late due to processor scheduling.  */
extern unsigned int alarm (unsigned int __seconds) __THROW;

/* Set an alarm to go off (generating a SIGALRM signal) in VALUE
   microseconds.  If INTERVAL is nonzero, when the alarm goes off, the
   timer is reset to go off every INTERVAL microseconds thereafter.
   Returns the number of microseconds remaining before the alarm.  */
extern __useconds_t ualarm (__useconds_t __value, __useconds_t __interval)
     __THROW;


/* Change the owner and group of FILE.  */
int chown (const char *__file, __uid_t __owner, __gid_t __group)
     __THROW __nonnull ((1)) __wur;

//#if defined __USE_XOPEN_EXTENDED || defined __USE_XOPEN2K8
/* Change the owner and group of the file that FD is open on.  */
int fchown (int __fd, __uid_t __owner, __gid_t __group) __THROW __wur;


/* Change owner and group of FILE, if it is a symbolic
   link the ownership of the symbolic link is changed.  */
int lchown (const char *__file, __uid_t __owner, __gid_t __group)
     __THROW __nonnull ((1)) __wur;



int fchdir (int __fd) __THROW __wur;


/* Execute the file FD refers to, overlaying the running program image.
   ARGV and ENVP are passed to the new program, as for `execve'.  */
int fexecve (int __fd, char *const __argv[], char *const __envp[])
     __THROW __nonnull ((2));


/* Get file-specific configuration information about PATH.  */
extern long int pathconf (const char *__path, int __name)
     __THROW __nonnull ((1));

/* Get file-specific configuration about descriptor FD.  */
extern long int fpathconf (int __fd, int __name) __THROW;

/* Get the value of the system variable NAME.  */
extern long int sysconf (int __name) __THROW;

/* Get the value of the string-valued system variable NAME.  */
extern size_t confstr (int __name, char *__buf, size_t __len) __THROW
    __attr_access ((__write_only__, 2, 3));


/* Get the process group ID of the calling process.  */
extern __pid_t getpgrp (void) __THROW;

/* Get the process group ID of process PID.  */
extern __pid_t __getpgid (__pid_t __pid) __THROW;
extern __pid_t getpgid (__pid_t __pid) __THROW;


/* Set the process group ID of the process matching PID to PGID.
   If PID is zero, the current process's process group ID is set.
   If PGID is zero, the process ID of the process is used.  */
int setpgid (__pid_t __pid, __pid_t __pgid) __THROW;

/* Both System V and BSD have `setpgrp' functions, but with different
   calling conventions.  The BSD function is the same as POSIX.1 `setpgid'
   (above).  The System V function takes no arguments and puts the calling
   process in its on group like `setpgid (0, 0)'.

   New programs should always use `setpgid' instead.

   GNU provides the POSIX.1 function.  */

/* Set the process group ID of the calling process to its own PID.
   This is exactly the same as `setpgid (0, 0)'.  */
int setpgrp (void) __THROW;



/* Create a new session with the calling process as its leader.
   The process group IDs of the session and the calling process
   are set to the process ID of the calling process, which is returned.  */
extern __pid_t setsid (void) __THROW;

/* Return the session ID of the given process.  */
extern __pid_t getsid (__pid_t __pid) __THROW;

/* Get the real user ID of the calling process.  */
extern __uid_t getuid (void) __THROW;

/* Get the effective user ID of the calling process.  */
extern __uid_t geteuid (void) __THROW;

/* Get the real group ID of the calling process.  */
extern __gid_t getgid (void) __THROW;

/* Get the effective group ID of the calling process.  */
extern __gid_t getegid (void) __THROW;

/* If SIZE is zero, return the number of supplementary groups
   the calling process is in.  Otherwise, fill in the group IDs
   of its supplementary groups in LIST and return the number written.  */
int getgroups (int __size, __gid_t __list[]) __THROW __wur
    __attr_access ((__write_only__, 2, 1));

/* Set the user ID of the calling process to UID.
   If the calling process is the super-user, set the real
   and effective user IDs, and the saved set-user-ID to UID;
   if not, the effective user ID is set to UID.  */
int setuid (__uid_t __uid) __THROW __wur;

/* Set the group ID of the calling process to GID.
   If the calling process is the super-user, set the real
   and effective group IDs, and the saved set-group-ID to GID;
   if not, the effective group ID is set to GID.  */
int setgid (__gid_t __gid) __THROW __wur;

/* Set the real group ID of the calling process to RGID,
   and the effective group ID of the calling process to EGID.  */
int setregid (__gid_t __rgid, __gid_t __egid) __THROW __wur;

/* Set the effective group ID of the calling process to GID.  */
int setegid (__gid_t __gid) __THROW __wur;

/* Fetch the real user ID, effective user ID, and saved-set user ID,
   of the calling process.  */
int getresuid (__uid_t *__ruid, __uid_t *__euid, __uid_t *__suid)
     __THROW;

/* Fetch the real group ID, effective group ID, and saved-set group ID,
   of the calling process.  */
int getresgid (__gid_t *__rgid, __gid_t *__egid, __gid_t *__sgid)
     __THROW;

/* Set the real user ID, effective user ID, and saved-set user ID,
   of the calling process to RUID, EUID, and SUID, respectively.  */
int setresuid (__uid_t __ruid, __uid_t __euid, __uid_t __suid)
     __THROW __wur;

/* Set the real group ID, effective group ID, and saved-set group ID,
   of the calling process to RGID, EGID, and SGID, respectively.  */
int setresgid (__gid_t __rgid, __gid_t __egid, __gid_t __sgid)
     __THROW __wur;


/* Clone the calling process, creating an exact copy.
   Return -1 for errors, 0 to the new process,
   and the process ID of the new process to the old process.  */
extern __pid_t fork (void) __THROWNL;

/* Clone the calling process, but without copying the whole address space.
   The calling process is suspended until the new process exits or is
   replaced by a call to `execve'.  Return -1 for errors, 0 to the new process,
   and the process ID of the new process to the old process.  */
extern __pid_t vfork (void) __THROW;


/* Return the pathname of the terminal FD is open on, or NULL on errors.
   The returned storage is good only until the next call to this function.  */
extern char *ttyname (int __fd) __THROW;

/* Store at most BUFLEN characters of the pathname of the terminal FD is
   open on in BUF.  Return 0 on success, otherwise an error number.  */
int ttyname_r (int __fd, char *__buf, size_t __buflen)
     __THROW __nonnull ((2)) __wur __attr_access ((__write_only__, 2, 3));

/* Return 1 if FD is a valid descriptor associated
   with a terminal, zero if not.  */
int isatty (int __fd) __THROW;

/* Return the index into the active-logins file (utmp) for
   the controlling terminal.  */
int ttyslot (void) __THROW;


/* Make a link to FROM named TO.  */
int link (const char *__from, const char *__to)
     __THROW __nonnull ((1, 2)) __wur;

/* Like link but relative paths in TO and FROM are interpreted relative
   to FROMFD and TOFD respectively.  */
int linkat (int __fromfd, const char *__from, int __tofd,
		   const char *__to, int __flags)
     __THROW __nonnull ((2, 4)) __wur;

/* Make a symbolic link to FROM named TO.  */
int symlink (const char *__from, const char *__to)
     __THROW __nonnull ((1, 2)) __wur;

/* Read the contents of the symbolic link PATH into no more than
   LEN bytes of BUF.  The contents are not null-terminated.
   Returns the number of characters read, or -1 for errors.  */
extern ssize_t readlink (const char *__restrict __path,
			 char *__restrict __buf, size_t __len)
     __THROW __nonnull ((1, 2)) __wur __attr_access ((__write_only__, 2, 3));

/* Like symlink but a relative path in TO is interpreted relative to TOFD.  */
int symlinkat (const char *__from, int __tofd,
		      const char *__to) __THROW __nonnull ((1, 3)) __wur;

/* Like readlink but a relative PATH is interpreted relative to FD.  */
extern ssize_t readlinkat (int __fd, const char *__restrict __path,
			   char *__restrict __buf, size_t __len)
     __THROW __nonnull ((2, 3)) __wur __attr_access ((__write_only__, 3, 4));

/* Remove the link NAME.  */
int unlink (const char *__name) __THROW __nonnull ((1));

/* Remove the link NAME relative to FD.  */
int unlinkat (int __fd, const char *__name, int __flag)
     __THROW __nonnull ((2));

/* Remove the directory PATH.  */
int rmdir (const char *__path) __THROW __nonnull ((1));


/* Return the foreground process group ID of FD.  */
extern __pid_t tcgetpgrp (int __fd) __THROW;

/* Set the foreground process group ID of FD set PGRP_ID.  */
int tcsetpgrp (int __fd, __pid_t __pgrp_id) __THROW;


/* Return the login name of the user.

   This function is a possible cancellation point and therefore not
   marked with __THROW.  */
extern char *getlogin (void);
/* Return at most NAME_LEN characters of the login name of the user in NAME.
   If it cannot be determined or some other error occurred, return the error
   code.  Otherwise return 0.

   This function is a possible cancellation point and therefore not
   marked with __THROW.  */
int getlogin_r (char *__name, size_t __name_len) __nonnull ((1))
    __attr_access ((__write_only__, 1, 2));

/* Set the login name returned by `getlogin'.  */
int setlogin (const char *__name) __THROW __nonnull ((1));


/* Put the name of the current host in no more than LEN bytes of NAME.
   The result is null-terminated if LEN is large enough for the full
   name and the terminator.  */
int gethostname (char *__name, size_t __len) __THROW __nonnull ((1))
    __attr_access ((__write_only__, 1, 2));

/* Set the name of the current host to NAME, which is LEN bytes long.
   This call is restricted to the super-user.  */
int sethostname (const char *__name, size_t __len)
     __THROW __nonnull ((1)) __wur __attr_access ((__read_only__, 1, 2));

/* Set the current machine's Internet number to ID.
   This call is restricted to the super-user.  */
int sethostid (long int __id) __THROW __wur;


/* Get and set the NIS (aka YP) domain name, if any.
   Called just like `gethostname' and `sethostname'.
   The NIS domain name is usually the empty string when not using NIS.  */
int getdomainname (char *__name, size_t __len)
     __THROW __nonnull ((1)) __wur __attr_access ((__write_only__, 1, 2));
int setdomainname (const char *__name, size_t __len)
     __THROW __nonnull ((1)) __wur __attr_access ((__read_only__, 1, 2));

/* Revoke access permissions to all processes currently communicating
   with the control terminal, and then send a SIGHUP signal to the process
   group of the control terminal.  */
int vhangup (void) __THROW;

/* Revoke the access of all descriptors currently open on FILE.  */
int revoke (const char *__file) __THROW __nonnull ((1)) __wur;


/* Enable statistical profiling, writing samples of the PC into at most
   SIZE bytes of SAMPLE_BUFFER; every processor clock tick while profiling
   is enabled, the system examines the user PC and increments
   SAMPLE_BUFFER[((PC - OFFSET) / 2) * SCALE / 65536].  If SCALE is zero,
   disable profiling.  Returns zero on success, -1 on error.  */
int profil (unsigned short int *__sample_buffer, size_t __size,
		   size_t __offset, unsigned int __scale)
     __THROW __nonnull ((1));


/* Turn accounting on if NAME is an existing file.  The system will then write
   a record for each process as it terminates, to this file.  If NAME is NULL,
   turn accounting off.  This call is restricted to the super-user.  */
int acct (const char *__name) __THROW;


/* Successive calls return the shells listed in `/etc/shells'.  */
extern char *getusershell (void) __THROW;
void endusershell (void) __THROW; /* Discard cached info.  */
void setusershell (void) __THROW; /* Rewind and re-read the file.  */


/* Put the program in the background, and dissociate from the controlling
   terminal.  If NOCHDIR is zero, do `chdir ("/")'.  If NOCLOSE is zero,
   redirects stdin, stdout, and stderr to /dev/null.  */
int daemon (int __nochdir, int __noclose) __THROW __wur;


/* Make PATH be the root directory (the starting point for absolute paths).
   This call is restricted to the super-user.  */
int chroot (const char *__path) __THROW __nonnull ((1)) __wur;

/* Prompt with PROMPT and read a string from the terminal without echoing.
   Uses /dev/tty if possible; otherwise stderr and stdin.  */
extern char *getpass (const char *__prompt) __nonnull ((1));




/* Return identifier for the current host.  */
extern long int gethostid (void);


/* Return the maximum number of file descriptors
   the current process could possibly have.  */
int getdtablesize (void) __THROW;


/* NOTE: These declarations also appear in <fcntl.h>; be sure to keep both
   files consistent.  Some systems have them there and some here, and some
   software depends on the macros being defined without including both.  */

/* `lockf' is a simpler interface to the locking facilities of `fcntl'.
   LEN is always relative to the current file position.
   The CMD argument is one of the following.

   This function is a cancellation point and therefore not marked with
   __THROW.  */

# define F_ULOCK 0	/* Unlock a previously locked region.  */
# define F_LOCK  1	/* Lock a region for exclusive use.  */
# define F_TLOCK 2	/* Test and lock a region for exclusive use.  */
# define F_TEST  3	/* Test a region for other processes locks.  */

int lockf (int __fd, int __cmd, __off_t __len) __wur;


/* Synchronize at least the data part of a file with the underlying
   media.  */
int fdatasync (int __fildes);

/* One-way hash PHRASE, returning a string suitable for storage in the
   user database.  SALT selects the one-way function to use, and
   ensures that no two users' hashes are the same, even if they use
   the same passphrase.  The return value points to static storage
   which will be overwritten by the next call to crypt.  */
extern char *crypt (const char *__key, const char *__salt)
     __THROW __nonnull ((1, 2));

/* Swab pairs bytes in the first N bytes of the area pointed to by
   FROM and copy the result to TO.  The value of TO must not be in the
   range [FROM - N + 1, FROM - 1].  If N is odd the first byte in FROM
   is without partner.  */
void swab (const void *__restrict __from, void *__restrict __to,
		  ssize_t __n) __THROW __nonnull ((1, 2))
    __attr_access ((__read_only__, 1, 3))
    __attr_access ((__write_only__, 2, 3));


/* Return the name of the controlling terminal.  */
extern char *ctermid (char *__s) __THROW;

/* Return the name of the current user.  */
extern char *cuserid (char *__s);

int pthread_atfork (void (*__prepare) (void),
			   void (*__parent) (void),
			   void (*__child) (void)) __THROW;

/* Write LENGTH bytes of randomness starting at BUFFER.  Return 0 on
   success or -1 on error.  */
int getentropy (void *__buffer, size_t __length) __wur
    __attr_access ((__write_only__, 1, 2));



#endif


#define PATH_MAX 4096



/* Values for the WHENCE argument to lseek.  */
# define SEEK_SET	0	/* Seek from beginning of file.  */
# define SEEK_CUR	1	/* Seek from current position.  */
# define SEEK_END	2	/* Seek from end of file.  */

extern off_t lseek (int __fd, int64_t __offset, int __whence);

int close (int __fd);


/* Read NBYTES into BUF from FD.  Return the
   number read, -1 for errors or 0 for EOF.
*/
extern size_t read (int __fd, void *__buf, size_t __nbytes);

/* Write N bytes of BUF to FD.  Return the number written, or -1.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern size_t write (int __fd, const void *__buf, size_t __n);


/* Read NBYTES into BUF from FD at the given position OFFSET without
   changing the file pointer.  Return the number read, -1 for errors
   or 0 for EOF.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern size_t pread (int fd, void* buf, size_t nbytes, off_t offset);


/* Write N bytes of BUF to FD at the given position OFFSET without
   changing the file pointer.  Return the number written, or -1.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern size_t pwrite (int fd, const void* buf, size_t n, off_t offset);



/* Make the process sleep for SECONDS seconds, or until a signal arrives
   and is not ignored.  The function returns the number of seconds less
   than SECONDS which it actually slept (thus zero if it slept the full time).
   If a signal handler does a `longjmp' or modifies the handling of the
   SIGALRM signal while inside `sleep' call, the handling of the SIGALRM
   signal afterwards is undefined.  There is no return value to indicate
   error, but if `sleep' returns SECONDS, it probably didn't work.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern unsigned int sleep (unsigned int __seconds);

/* Sleep USECONDS microseconds, or until a signal arrives that is not blocked
   or ignored.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
int usleep (uint64_t __useconds);


/* NULL-terminated array of "NAME=VALUE" environment variables.  */
extern char **__environ;


/* Terminate program execution with the low-order 8 bits of STATUS.  */
void _exit (int __status) __attribute__ ((__noreturn__));



/* Return the number of bytes in a page.  This is the system's page size,
   which is not necessarily the same as the hardware page size.  */
int getpagesize (void)  __attribute__ ((__const__));



/* Set the end of accessible data space (aka "the break") to ADDR.
   Returns zero on success and -1 for errors (with errno set).  */
int brk (void *__addr);

/* Increase or decrease the end of accessible data space by DELTA bytes.
   If successful, returns the address the previous end of data space
   (i.e. the beginning of the new space, if DELTA > 0);
   returns (void *) -1 for errors (with errno set).  */
void *sbrk (uint64_t __delta);


/* Execute PATH with arguments ARGV and environment from `environ'.  */
int execv (const char *__path, char const *const __argv[]);

/* Replace the current process, executing PATH with arguments ARGV and
   environment ENVP.  ARGV and ENVP are terminated by NULL pointers.  */
int execve (const char *__path, char const *const __argv[],
		   char *const __envp[]);

/* Execute PATH with all arguments after PATH until a NULL pointer,
   and the argument after that for environment.  */
int execle (const char *__path, const char *__arg, ...);

/* Execute PATH with all arguments after PATH until
   a NULL pointer and environment from `environ'.  */
int execl (const char *__path, const char *__arg, ...);

/* Execute FILE, searching in the `PATH' environment variable if it contains
   no slashes, with arguments ARGV and environment from `environ'.  */
int execvp (const char *__file, char const *const __argv[]);

int execvpe(const char *file, char const *const argv[],
                       char const *const envp[]);


/* Execute FILE, searching in the `PATH' environment variable if
   it contains no slashes, with all arguments after FILE until a
   NULL pointer and environment from `environ'.  */
int execlp (const char *__file, const char *__arg, ...);




/* Change the process's working directory to PATH.  */
int chdir (const char *__path);

/* Get the pathname of the current working directory,
   and put it in SIZE bytes of BUF.  Returns NULL if the
   directory couldn't be determined or SIZE was too small.
   If successful, returns BUF.  In GNU, if BUF is NULL,
   an array is allocated with `malloc'; the array is SIZE
   bytes long, unless SIZE == 0, in which case it is as
   big as necessary.  */
// Bincows implementation of getcwd is similar to GNU:
// if buf == 0 && size == 0, return a malloced buffer
// with the right size.
extern char *getcwd (char *__buf, size_t __size);


/* Get the process ID of the calling process.  */
extern pid_t getpid (void);

/* Get the process ID of the calling process's parent.  */
extern pid_t getppid (void);


typedef enum open_flags {
    O_RDONLY    = 1,
    O_WRONLY    = 2,
    O_RDWR      = 3,
    O_CREAT     = 4,
    O_EXCL      = 8,
    O_TRUNC     = 16,
    O_APPEND    = 32,
    O_NONBLOCK  = 64,
    O_DIRECTORY = 128,
    O_NOFOLLOW  = 256,
    O_DIRECT    = 512,
    O_DIR       = 4096,
} open_flags_t;


/* Invoke `system call' number SYSNO, passing it the remaining arguments.
   This is completely system-dependent, and not often useful.

   In Unix, `syscall' sets `errno' for all errors and most calls return -1
   for errors; in many systems you cannot pass arguments or get return
   values for all system calls (`pipe', `fork', and `getppid' typically
   among them).

   In Mach, all system calls take normal arguments and always return an
   error code (zero for success).  */
extern uint64_t syscall (long int __sysno, const void* args, size_t args_sz);



/* Duplicate FD, returning a new file descriptor on the same file.  */
int dup (int __fd);

/* Duplicate FD to FD2, closing FD2 and making it open on the same file.  */
int dup2 (int __fd, int __fd2);


#include "sys/stat.h"


int open (const char *pathname, int flags, mode_t mode);



/* Values for the second argument to access.
   These may be OR'd together.  */
#define	R_OK	4		/* Test for read permission.  */
#define	W_OK	2		/* Test for write permission.  */
#define	X_OK	1		/* Test for execute permission.  */
#define	F_OK	0		/* Test for existence.  */

/* Test for access to NAME using the real UID and real GID.  */
int access (const char* name, int type);



/* Truncate FILE to LENGTH bytes.  */
int truncate (const char* file, off_t length);

/* Truncate the file FD is open on to LENGTH bytes.  */
int ftruncate (int fd, off_t length);



/* Create a one-way communication channel (pipe).
   If successful, two file descriptors are stored in PIPEDES;
   bytes written on PIPEDES[1] can be read from PIPEDES[0].
   Returns 0 if successful, -1 if not.  */
int pipe (int pipedes[2]);



/* Make all changes done to FD actually appear on disk.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
int fsync (int __fd);

/* Make all changes done to all files actually appear on disk.  */
void sync (void);



/* Suspend the process until a signal arrives.
   This always returns -1 and sets `errno' to EINTR.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
int pause (void);


#ifdef __cplusplus
}
#endif


#endif /* unistd.h  */
