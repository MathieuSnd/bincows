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


#define CURSOR_CHAR '_'

// initialized in parse_args
static char* opened_filename = NULL;


// move cursor to the top of the screen
static const char* cursor_reset_seq = "\033[0;0H";


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



    // layout buffer
    layout->buf_sz  = layout->lines * layout->cols * 2 + 2 * strlen(cursor_reset_seq) + 1;
    layout->buf     = malloc(layout->buf_sz);


    return layout;
}


void write_padding(char* string, int line) {
#define MAX_DIGITS LEFT_PADDING - 2

    char str_line[MAX_DIGITS + 1];

    int digits = snprintf(str_line, MAX_DIGITS + 1, "%u", line);

    int num_begin = LEFT_PADDING - 1 - digits;

    memset(string, '-', num_begin);
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


char* footer_message = "footer message";

void print_footer(char* buf, file_buffer_t* fb, screen_layout_t* sl) {

    buf += sl->cols * (sl->lines - sl->bottom_padding);

    // second last line
    memset(buf , '~', sl->cols);

    
    // center the footer message
    if(footer_message) {
        buf += sl->cols;

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




// update cursor position:
// put the right character at its last position
// and put the cursor char at its new one
void update_cursor(file_buffer_t* fb, screen_layout_t* sl, 
                   cursor_t* new, cursor_t* last) {
        
    char buf[64];


    int lastx = last->col + LEFT_PADDING,
        lasty = last->row + sl->top_padding;

    // character that is hidden behind
    // the last cursor
    char hidden = sl->buf[strlen(cursor_reset_seq) + lasty * sl->cols + lastx];


    int len = snprintf(buf, sizeof(buf), 
            "\033[%u;%uH%c" // put new cursor
            "\033[%u;%uH%c" // remove last cursor
            "\033[0;0H",    // back to 0:0

            new->col + LEFT_PADDING + 1, new->row + sl->top_padding + 1,
            CURSOR_CHAR,

            lastx + 1, lasty + 1,
            hidden
    );

    fwrite(buf, 1, len, stdout);
}



// draw the whole screen
void draw_screen(
        file_buffer_t* fb, 
        screen_layout_t* sl, 
        cursor_t* cur
) {

    // compute end line to draw
    size_t endline = sl->lines - sl->top_padding - sl->bottom_padding;

    if(endline > fb->newlines_count ) {
        endline = fb->newlines_count;
    }


    // @todo buffer shift
    char* buf_begin = fb->buffer + fb->newlines[0];
    char* buf_end = fb->buffer + fb->newlines[0 + endline];


    char*  screen_begin = sl->buf + strlen(cursor_reset_seq);


    memcpy(sl->buf, cursor_reset_seq, strlen(cursor_reset_seq));


    int lnum = fb->buffer_line_begin + 0; // @todo buffer shift


    print_header(screen_begin, fb, sl);

    char* curline_begin = screen_begin + sl->top_padding * sl->cols;

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

    print_footer(screen_begin, fb, sl);


    // buffer end
    curline_begin = screen_begin + sl->lines * sl->cols - 1;

    int cursor_line_begin = (cur->row + sl->top_padding) * sl->cols;
    int cursor_pos        = cursor_line_begin + cur->col + LEFT_PADDING;

    //screen_begin[cursor_pos] = 'X';

    memcpy(curline_begin, cursor_reset_seq, sizeof(cursor_reset_seq));


    


    //size_t len = buf_end - buf_begin;
    // draw the buffer
    fwrite(sl->buf, 1, sl->buf_sz, stdout);
    //printf("_");
    // print cursor
    //printf("\x1b[%u;%uHX", cur->col + LEFT_PADDING + 1, cur->row + sl->top_padding + 1);
    fflush(stdout);

}


static void load_screen_size(void) {
    terminfo_t ti;

    if(terminfo_read(&ti) == -1) {
        fprintf(stderr, "Could not read terminfo\n");
        exit(1);
    }

    screen_size.width = ti.cols;
    screen_size.height = ti.lines;
}




void ask_exit(void) {
    exit(1);
}



void sigterm(int n) {
    ask_exit();
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

    cursor_t old_cur;

    atexit(cleanup);


    draw_screen(fb, sl, &cur);

    while(1) {
        int c = getc(stdin);

        old_cur = cur;
        int modified = read_input(c, fb, &cur, sl);


        switch(modified) {
            case 1: 
                draw_screen(fb, sl, &cur);
                update_cursor(fb, sl, &cur, &old_cur);
                break;
            default:
                update_cursor(fb, sl, &cur, &old_cur);
        }
        
    }
}
