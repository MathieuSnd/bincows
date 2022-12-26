#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct FILE {
    int    fd;
    int    flags;
    int    mode;
    int    ungetc;
    int    error;
    int    eof;
    int    pos;
    size_t size;
    size_t bufsize;
    char*  buffer;
    size_t buf_off;

    // 0 if the buffer was not read/written
    // 1 if the buffer was read to 
    // 2 if the buffer was written to
    int buff_state;
};

FILE *stdin;
FILE *stdout;
FILE *stderr;

void __stdio_init(void) {
    FILE* std = (FILE*)malloc(sizeof(FILE) * 3);
    stdin  = std + 0;
    stdout = std + 1;
    stderr = std + 2;

    *stdin = (FILE){
        .fd = STDIN_FILENO,
        .flags = O_RDONLY,
        .mode = _IONBF,
        .ungetc = -1,
        .error = 0,
        .eof = 0,
        .pos = 0,
        .size = 0,
        .bufsize = 0,
        .buffer = NULL,
        .buf_off = 0,
        .buff_state = 0,
    };

    *stdout = (FILE){
        .fd = STDOUT_FILENO,
        .flags = O_WRONLY,
        .mode = _IONBF,
        .ungetc = -1,
        .error = 0,
        .eof = 0,
        .pos = 0,
        .size = 0,
        .bufsize = 0,
        .buffer = NULL,
        .buf_off = 0,
        .buff_state = 0,
    };

    *stderr = (FILE){
        .fd = STDERR_FILENO,
        .flags = O_WRONLY,
        .mode = _IONBF,
        .ungetc = -1,
        .error = 0,
        .eof = 0,
        .pos = 0,
        .size = 0,
        .bufsize = 0,
        .buffer = NULL,
        .buf_off = 0,
        .buff_state = 0,
    };
}

#define BUF_NONE 0
#define BUF_RD 1
#define BUF_WR 2


// return -1 if invalid modes
static
int modes_to_flags(const char *restrict modes) {
    int flags = 0;

    // compute flags
    if(modes[0] == 'r')
        flags |= O_RDONLY;
    else if(modes[0] == 'w') {
        flags |= O_WRONLY;
        flags |= O_CREAT;
        flags |= O_TRUNC;
    }
    else if(modes[0] == 'a') {
        flags |= O_WRONLY;
        flags |= O_CREAT;
        flags |= O_APPEND;
    }
    else
        return -1;

    int i = 1;
    
    if(modes[i] == 'b')
        i++;
    

    if(modes[i] == '+')
        flags |= O_RDWR;
    else if(modes[i] != 0)
        return -1;
    
    return flags;
}

static 
FILE* create_file_struct(int fd, int flags) {
    FILE* file = malloc(sizeof(FILE));
    if(!file) {
        return NULL;
    }


    file->fd = fd;
    file->flags = flags;
    file->mode = _IONBF;
    file->ungetc = 0;
    file->error = 0;
    file->eof = 0;
    file->pos = 0;
    file->size = 0;
    file->bufsize = BUFSIZ;
    file->buf_off = 0;
    file->buffer = malloc(BUFSIZ);

    if(!file->buffer) {
        free(file);
        return NULL;
    }


    return file;

}


FILE *fopen (const char *restrict filename,
		     const char *restrict modes
) {

    int flags = 0;
    int fd;

    flags = modes_to_flags(modes);
    if(flags == -1)
        return NULL;

    fd = open(filename, flags, 0);
    if(fd < 0) {
        return NULL;
    }

    return create_file_struct(fd, flags);
}


FILE* fdopen(int fd, const char* modes) {
    int flags = modes_to_flags(modes);
    if(flags == -1)
        return NULL;

    if(fd < 0 || fd > FOPEN_MAX)
        return NULL;

    return create_file_struct(fd, flags);
}


int fflush (FILE * restrict stream) {
    if(stream->buf_off) {
        if(stream->buff_state == BUF_WR)
            write(stream->fd, stream->buffer, stream->buf_off);
        
        else if(stream->buff_state == BUF_RD)
            stream->bufsize = read(stream->fd, stream->buffer, stream->buf_off);

        stream->buf_off = 0;
    }

    return 0;
}


int fclose (FILE* stream) {
    fflush(stream);
    close(stream->fd);
    free(stream->buffer);
    free(stream);

    return 0;
}


 FILE *freopen (const char* restrict filename,
                const char* restrict modes,
                FILE* restrict stream
) { 
    (void) modes;

    fflush(stream);
    close(stream->fd);
    stream->fd = open(filename, 0, 0);
    if(stream->fd < 0) {
        return NULL;
    }
    return stream;
}



int fgetc(FILE* stream) {
    if((stream->flags & O_RDONLY) == 0) {
        printf("fgetc on write only\n");
        return EOF;
    }

    //printf("fgetc fd %u\n", stream->fd);

    switch(stream->mode) {
        case _IOFBF: 
        printf("fgetc on _IOFBF\n");
            return EOF;
        case _IOLBF:
        printf("fgetc on _IOLBF\n");
            return EOF;
        case _IONBF: {
            int c;
            stream->pos++;
            int n = read(stream->fd, &c, 1);
            if(n == 1) {
                return (unsigned char)c;
            }
        //printf("fgetc on n = %u\n", n);
            return EOF;
        }
        default:
            return EOF;
    }
}


int fputc (int c, FILE* stream) {
    char s[2];
    s[0] = c;
    s[1] = '\0';
    
    return fputs(s, stream);
}


int putchar(int c) {
    return fputc(c, stdout);
}

int puts (const char *s) {
    char* buf = malloc(strlen(s) + 1);

    memcpy(buf, s, strlen(s));
    buf[strlen(s)] = '\n';

    int res =  write(STDOUT_FILENO, buf, strlen(s) + 1);

    free(buf);

    if(res <= 0) {
        return EOF;
    }
    return 0;
}


size_t fread (void *restrict ptr, size_t size,
		     size_t n, FILE *restrict stream
) {
    if((stream->flags & O_RDONLY) == 0) {
        stream->error = 1;
        return 0;
    }

    void* buf = malloc(size * n);

    int64_t rd = read(stream->fd, buf, size * n);

    if(rd < 0) {
        stream->error = 1;
        return 0;
    }


    if(rd == 0)
        stream->eof = 1;

    rd /= size;


    memcpy(ptr, buf, rd * size);


    stream->pos += n * size;


    return rd;
}


size_t fwrite (const void *restrict ptr, size_t size,
              size_t _n, FILE *restrict stream
) {
    if((stream->flags & O_WRONLY) == 0) {
        stream->error = 1;
        return 0;
    }

    int64_t n = write(stream->fd, ptr, size * _n);



    if(n < 0) {
        stream->error = 1;
        return 0;
    }

    if(n != (int64_t)size * n)
        stream->eof = 1;

    stream->pos += n;
    return n / size;
}


int fseek (FILE* stream, long offset, int whence) {
    int seeked = lseek(stream->fd, offset, whence);

    stream->pos = seeked;

    

    return (seeked == -1) ? -1: 0;
}


long ftell (FILE* stream) {
    return stream->pos;
}

void rewind (FILE* stream) {
    fseek(stream, 0, SEEK_SET);
}

int fgetpos (FILE *restrict stream, fpos_t *restrict pos) {
    *pos = stream->pos;
    return 0;
}

int fileno (FILE *stream) {
    return stream->fd;
}

int feof (FILE *stream) {
    return stream->eof;
}

int ferror(FILE* stream) {
    return stream->error;
}


int fputs(const char* restrict s, FILE* restrict stream) {
    if((stream->flags & O_WRONLY) == 0) {
        printf("ERROR: FPUTS ON READ ONLY\n");
        return EOF;
    }

    switch(stream->mode) {
        case _IOFBF: {
            printf("ERROR: FPUTS ON _IOFBF\n");
            return EOF;
        }
        case _IOLBF: {
            printf("ERROR: FPUTS ON _IOFBF\n");
            return EOF;
        }
        case _IONBF: {
            size_t n = write(stream->fd, s, strlen(s));
            if(n == strlen(s)) {
                return 0;
            }
            stream->eof = 1;
            return EOF;
        }
        default:
            return EOF;
    }
}

// impl of getline using fgetc
ssize_t getline(char **restrict lineptr, size_t *restrict n,
                       FILE *restrict stream
) {

    if((stream->flags & O_RDONLY) == 0) {
        return -1;
    }

    char* buf = malloc(BUFSIZ);
    if(!buf) {
        return -1;
    }

    char* p = buf;
    size_t size = BUFSIZ;
    size_t len = 0;
    int c = 0;
    while((c = fgetc(stream)) != EOF) {
        if(len == size) {
            size *= 2;
            char* new_buf = realloc(buf, size);
            if(!new_buf) {
                free(buf);
                return -1;
            }
            buf = new_buf;
            p = buf + len;
        }
        *p++ = c;
        len++;
        if(c == '\n') {
            break;
        }
    }
    if(c == EOF && len == 0) {
        free(buf);
        return -1;
    }
    *p = '\0';
    *lineptr = buf;
    *n = len;
    return len;
}

char* fgets(char* restrict s, int n, FILE* restrict stream) {
    if((stream->flags & O_RDONLY) == 0) {
        return NULL;
    }

    switch(stream->mode) {
        case _IOFBF: {
            return NULL;
        }
        case _IOLBF: {
            return NULL;
        }
        case _IONBF: {
            int i = 0;
            int eof = 0;
            n--;
            while(i < n) {
                char c = fgetc(stream);
                if(c == EOF)
                    eof = 1;

                if(eof|| c == '\n')
                    break;

                s[i] = c;
                i++;
            }

            s[i] = '\0';
            return (eof && i == 0) ? NULL : s;
        }
        default:
            return NULL;
    }
}


int fprintf (FILE* restrict stream, const char* restrict format, ...) {
    va_list args;
    va_start(args, format);
    int n = vfprintf(stream, format, args);
    va_end(args);
    return n;
}


int vfprintf(FILE* stream, const char* restrict format, va_list arg) {
    char* buf = malloc(BUFSIZ);
    if(!buf) {
        return -1;
    }

    int n = vsprintf(buf, format, arg);
    if(n < 0) {
        free(buf);
        return -1;
    }

    int res = write(stream->fd, buf, n);

    if(res <= 0) {
        stream->error = 1;
    }
    else
        stream->pos += n;


    return n;
}

int vprintf(const char* restrict format, va_list arg) {

    char* buf = malloc(BUFSIZ);
    if(!buf) {
        return -1;
    }

    int n = vsprintf(buf, format, arg);


    write(1, buf, strlen(buf));

    free(buf);

    return n;
}

int printf(const char* restrict format, ...) {
    va_list args;
    va_start(args, format);
    int n = vprintf(format, args);
    va_end(args);
    return n;
}

void perror (const char* s) {
    write(2, s, strlen(s));
    write(2, ": ", 2);
    write(2, strerror(errno), strlen(strerror(errno)));
    write(2, "\n", 1);
}