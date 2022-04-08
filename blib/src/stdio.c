#include <stdio.h>
#include <unistd.h>
#include <alloc.h>


struct FILE {
    int fd;
    int flags;
    int mode;
    int ungetc;
    int error;
    int eof;
    int pos;
    int size;
    char* buffer;
    size_t buf_off;
};


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
    file->buf_off = 0;
    file->buffer = malloc(BUFSIZ);

    return file;                 
}


int fflush (FILE *stream) {
    if(stream->buf_off) {
        write(stream->fd, stream->buffer, stream->buf_off);
        stream->buf_off = 0;
    }
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

void setbuf (FILE *restrict stream, char *restrict buf) {
    free(stream->buffer);
    stream->buffer = buf;
}



