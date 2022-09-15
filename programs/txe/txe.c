#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <terminfo.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>


#include "txe.h"


#define VERSION_STR "txe v0.1"

// initialized in parse_args
static char* opened_filename = NULL;


//#define MAX_BUFF_SIZE (1024 * 1024 * 512)


struct screen_size screen_size;


static FILE* parse_args(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        exit(1);
    } 

    if(!strcmp(argv[1], "-")) {
        opened_filename = strdup("stdin");
        return stdin;
    } else {
        FILE* f = fopen(argv[1], "r");
        if(!f) {
            fprintf(stderr, "Could not open file %s\n", argv[1]);
            exit(1);
        }

        opened_filename = strdup(argv[1]);
        
        return f;
    }

}


#define LEFT_PADDING 6


void analyze_lines(file_buffer_t* buf) {

    // find all newlines in the buffer
    buf->newlines = realloc(buf->newlines, sizeof(uint32_t) * (buf->buffer_size+2));

    if(!buf->newlines) {
        fprintf(stderr, "Could not allocate memory for the newlines array\n");
        exit(1);
    }


    buf->newlines_count = 1;

    buf->newlines[0] = 0;

    size_t row = 0;
    for(size_t i = 0; i < buf->buffer_size; i++) {
        
        if(buf->buffer[i] == '\n') {
            row = LEFT_PADDING;
            buf->newlines[buf->newlines_count] = i;
            buf->newlines_count++;
        }

        if(buf->buffer[i] == '\t') {
            row += 8 - (row % 8);
        }

        if(row == screen_size.width) {
            row = LEFT_PADDING;
            buf->newlines[buf->newlines_count] = i;
            buf->newlines_count++;
        }

        row++;
    }

    buf->newlines[buf->newlines_count] = buf->buffer_size;

    // shrink buffer
    buf->newlines = realloc(buf->newlines, sizeof(uint32_t) * (buf->newlines_count + 1));

}


file_buffer_t* load_file(FILE* f) {

    file_buffer_t* buf = malloc(sizeof(file_buffer_t));
 
    
    // try to load the entiere 
    fseek(f, 0, SEEK_END);
    buf->buffer_size = ftell(f) + 1;
    fseek(f, 0, SEEK_SET);


    buf->buffer = malloc(buf->buffer_size);

    if(!buf->buffer) {
        fprintf(stderr, "Could not allocate memory for the buffer\n");
        exit(1);
    }

    size_t read_bytes = fread(buf->buffer, 1, buf->buffer_size - 1, f);


    buf->buffer_size = read_bytes;


    // shrink buffer
    buf->buffer   = realloc(buf->buffer, buf->buffer_size);

    buf->newlines = NULL;
    analyze_lines(buf);

    // start on line 1 
    buf->buffer_line_begin = 1;

    return buf;
}



screen_layout_t* init_screen_layout(void) {
    screen_layout_t* layout = malloc(sizeof(screen_layout_t));

    layout->top_padding = 2;
    layout->bottom_padding = 2;
    layout->lines = screen_size.height;
    layout->cols = screen_size.width;


    return layout;
}


void write_padding(char* string, int line) {
#define MAX_DIGITS LEFT_PADDING - 2

    char str_line[MAX_DIGITS + 1];

    int digits = snprintf(str_line, MAX_DIGITS + 1, "%u", line);

    int num_begin = LEFT_PADDING - 1 - digits;

    memset(string, ' ', num_begin);
    memcpy(string + num_begin, str_line, digits);

    string[LEFT_PADDING -1] = '|';
}






void print_header(char* buf, file_buffer_t* fb, screen_layout_t* sl) {
    size_t header_len_max = sl->cols;
    char* header_string = malloc(header_len_max);

    int len = snprintf(header_string, header_len_max, "%s - %s", VERSION_STR, opened_filename);

    // center the headline

    int left  = (sl->cols - len) / 2;
    int right = left + len;

    // 1st line
    memset(buf, ' ', left);
    memcpy(buf + left, header_string, len);
    memset(buf + right, ' ', sl->cols - right);

    // 2nd line
    memset(buf + sl->cols, '~', sl->cols);

    assert(sl->top_padding == 2);


}


char* footer_message = NULL;

void print_footer(char* buf, file_buffer_t* fb, screen_layout_t* sl) {

    buf += sl->cols * (sl->lines - sl->bottom_padding);

    // second last line
    memset(buf , '~', sl->cols * 2);

    
    // center the footer message
    if(footer_message) {
        size_t len = strlen(footer_message);

        int left  = (sl->cols - len) / 2;
        int right = left + len;

        // last line
        memset(buf, ' ', left);
        memcpy(buf + left, footer_message, len);
        memset(buf + right, ' ', sl->cols - right);
    }

    assert(sl->bottom_padding == 2);
}




void draw_screen(file_buffer_t* fb, screen_layout_t* sl, cursor_t* cur) {

    // compute end line to draw
    size_t endline = sl->lines - sl->top_padding - sl->bottom_padding;

    if(endline > fb->newlines_count ) {
        endline = fb->newlines_count;
    }
    // move cursor to the top of the screen
    printf("\033[0;0H");

    const char* end_seq = "\033[0;0H";


    // @todo buffer shift
    char* buf_begin = fb->buffer + fb->newlines[0];
    char* buf_end = fb->buffer + fb->newlines[0 + endline];


    // layout buffer
    size_t layout_buf_sz =  sl->lines * sl->cols * 2 + sizeof(end_seq);
    char*  layout_buf    = malloc(layout_buf_sz);


    int lnum = fb->buffer_line_begin + 0; // @todo buffer shift


    print_header(layout_buf, fb, sl);

    char* curline_begin = layout_buf + sl->top_padding * sl->cols;

    for(size_t l = 0; l < sl->lines - sl->top_padding - sl->bottom_padding; l++) {

        if(l >= endline) {
            // end of file
            memset(curline_begin, ' ', screen_size.width);
        }
        else {
            int line_size = fb->newlines[l + 1] - fb->newlines[l];

            char* line_begin = fb->buffer + fb->newlines[l];

            if(line_size && *line_begin == '\n') {
                line_begin++;
                line_size--;
            }

            assert(line_size >= 0);

            char padding[LEFT_PADDING];

            write_padding(padding, lnum);

            memcpy(curline_begin, padding, LEFT_PADDING);

            memcpy(curline_begin + LEFT_PADDING, line_begin, line_size);

            // replace weird characters by '?'
            for(int i = 0; i < line_size; i++) {
                char c = curline_begin[LEFT_PADDING + i];

                if(!isascii(c) || iscntrl(c))
                    curline_begin[LEFT_PADDING + i] = '?';
            }

            // flush the rest of the line
            memset(
                curline_begin + LEFT_PADDING + line_size, 
                ' ', 
                screen_size.width - line_size
            );

            lnum++;
        }

        curline_begin += screen_size.width;
    }

    print_footer(layout_buf, fb, sl);


    // buffer end
    curline_begin = layout_buf + sl->lines * sl->cols - 1;
//
    memcpy(curline_begin, end_seq, sizeof(end_seq));


    


    //size_t len = buf_end - buf_begin;
    // draw the buffer
    fwrite(layout_buf, 1, layout_buf_sz, stdout);
    //printf("_");
    // print cursor
    printf("\x1b[%u;%uHX", cur->col + LEFT_PADDING + 1, cur->row + sl->top_padding + 1);
    fflush(stdout);

    free(layout_buf);
}


static void load_screen_size(void) {
    terminfo_t ti;

    if(terminfo_read(&ti) == -1) {
        fprintf(stderr, "Could not read terminfo\n");
        exit(1);
    }

    printf("bordel de\n");

    screen_size.width = ti.cols;
    screen_size.height = ti.lines;
    printf("bordel de\n");
}



void sigterm(int n) {
    ask_exit();
}


void ask_exit(void) {
    exit(1);
}


void cleanup(void) {
    printf("\x0c");
}




int main(int argc, char** argv) {

    signal(SIGINT, sigterm);
    signal(SIGTERM, sigterm);



    FILE* file = parse_args(argc, argv);
    assert(file);

    load_screen_size();



    file_buffer_t* fb = load_file(file);


    screen_layout_t* sl = init_screen_layout();


    cursor_t cur = {
        .file_offset = 0,
        .row = 0,
        .col = 0,
    };

    atexit(cleanup);
    //printf("\x0c");
    //printf("\033[0;0H");




    while(1) {
        draw_screen(fb, sl, &cur);
        int c = getc(stdin);
        read_input(c, fb, &cur, sl);
    }
}
