#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

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

void stio_init(void) {
    FILE* std = (FILE*)malloc(sizeof(FILE) * 3);
    stdin  = std + 0;
    stdout = std + 1;
    stderr = std + 2;

    *stdin = (FILE){
        .fd = STDIN_FILENO,
        .flags = 0,
        .mode = 0,
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
        .flags = 0,
        .mode = 0,
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
        .flags = 0,
        .mode = 0,
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


FILE *fopen (const char *restrict filename,
		     const char *restrict modes
) {

    int fd = open(filename, 0, 0);
    if(fd < 0) {
        return NULL;
    }

    FILE* file = malloc(sizeof(FILE));
    if(!file) {
        return NULL;
    }


    file->fd = fd;
    file->flags = 0;
    file->mode = 0;
    file->ungetc = 0;
    file->error = 0;
    file->eof = 0;
    file->pos = 0;
    file->size = 0;
    file->bufsize = BUFSIZ;
    file->buf_off = 0;
    file->buffer = malloc(BUFSIZ);

    return file;                 
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
}


 FILE *freopen (const char* restrict filename,
                const char* restrict modes,
                FILE* restrict stream
) { 
    fflush(stream);
    close(stream->fd);
    stream->fd = open(filename, 0, 0);
    if(stream->fd < 0) {
        return NULL;
    }
    return stream;
}




int fgetc(FILE* stream) {
    if(stream->flags & O_WRONLY) {
        return EOF;
    }

    switch(stream->mode) {
        case _IOFBF: 
            return EOF;
        case _IOLBF:
            return EOF;
        case _IONBF: {
            char c;
            stream->pos++;
            int n = read(stream->fd, &c, 1);
            if(n == 1) {
                return c;
            }
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
    if(write(STDOUT_FILENO, s, strlen(s)) < 0) {
        return EOF;
    }
    else if(write(STDOUT_FILENO, "\n", 1) < 0) {
        return EOF;
    }
    return 0;
}


size_t fread (void *restrict ptr, size_t size,
		     size_t n, FILE *restrict stream
) {
    if(stream->flags & O_WRONLY) {
        return 0;
    }


    int64_t rd = read(stream->fd, ptr, size * n);

    if(rd < 0) {
        return 0;
    }


    if(rd != size * n)
        stream->eof = 1;

    stream->pos += n;


    return rd / size;
}


size_t fwrite (const void *restrict ptr, size_t size,
              size_t _n, FILE *restrict stream
) {
    if(stream->flags & O_RDONLY) {
        return 0;
    }

    int64_t n = write(stream->fd, ptr, size * _n);



    if(n < 0) {
        return 0;
    }

    if(n != size * n)
        stream->eof = 1;

    stream->pos += n;
    return n / size;
}


int fseek (FILE* stream, long offset, int whence) {
    int seeked = lseek(stream->fd, offset, whence);

    stream->pos = seeked;

    return seeked;
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


int fputs(const char* restrict s, FILE* restrict stream) {
    if(stream->flags & O_RDONLY) {
        return EOF;
    }

    switch(stream->mode) {
        case _IOFBF: {
            return EOF;
        }
        case _IOLBF: {
            return EOF;
        }
        case _IONBF: {
            int n = write(stream->fd, s, strlen(s));
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

char* fgets(char* restrict s, int n, FILE* restrict stream) {
    if(stream->flags & O_WRONLY) {
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
            while(i < n) {
                char c = fgetc(stream);
                if(c == EOF) {
                    return NULL;
                }
                s[i] = c;
                i++;
            }
            return s;
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
        return -1;
    }

    write(stream->fd, buf, n);

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

