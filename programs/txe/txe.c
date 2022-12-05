#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <ctype.h>


#include "terminal.h"
#include "txe.h"


#define VERSION_STR "txe v0.1"


#define CURSOR_CHAR '_'

// initialized in parse_args
static char* opened_filename = NULL;


// move cursor to the top of the screen
static const char* cursor_reset_seq = "\033[0;0H";


//#define MAX_BUFF_SIZE (1024 * 1024 * 512)


struct screen_size screen_size;


static void show_help(char* arg0) {
    fprintf(stdout, 
        "Usage: %s [options] <file>\n"
        "Options: \n"
        "    -c, --create    create the file if it doesn't already exist\n"
        "    -h, --help      show this message\n"
        "\n"
        "For Bug reporting, please leave an issue on github.com/MathieuSnd/bincows.\n"
        "\n"
        , arg0);
}


static FILE* parse_args(int argc, char** argv) {

    int create = 0;

    for(int i = 1; i < argc; i++) {
        char* arg = argv[i];

        if(arg[0] != '-') {
            // must be a filename
            if(opened_filename) {
                fprintf(stderr, "%s: multiple input files", argv[0]);
                exit(1);
            }
            else 
                opened_filename = strdup(arg);
        }
        else {
            if(!strcmp(arg, "-c") || !strcmp(arg, "--create"))
                create = 1;
            else if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
                show_help(argv[0]);
                exit(0);
            }
        }
    }

    if(argc == 1) {
        show_help(argv[0]);
        exit(0);
    }


    // try to open the file
    FILE* f = fopen(opened_filename, "r");
    if(!f) {
        if(create) {
            f = fopen(opened_filename, "w+");
            if(f)
                return f;
        }
        fprintf(stderr, "Could not open file %s\n", opened_filename);
        exit(1);
    }

    return f;
}




void file_analyze_lines(file_buffer_t* buf) {

    // find all newlines in the buffer
    buf->newlines = realloc(buf->newlines, sizeof(uint32_t) * (buf->buffer_size+2));

    if(!buf->newlines) {
        fprintf(stderr, "Could not allocate memory for the newlines array %u\n", buf->buffer_size);
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

    
    int res;
    // try to load the entiere 
    res = fseek(f, 0, SEEK_END);

    if(res) {
        printf("couldn't seek file %s\n", opened_filename);
        exit(1);
    }

    buf->buffer_size = ftell(f) + 1;
    fseek(f, 0, SEEK_SET);


    if(buf->buffer_size <= 0) {
        fprintf(stderr, "IO Error reading %s\n", opened_filename);
        exit(1);
    }



    buf->buffer = malloc(buf->buffer_size);

    if(!buf->buffer) {
        fprintf(stderr, "Could not allocate memory for the buffer\n");
        exit(1);
    }

    size_t read_bytes = fread(buf->buffer, 1, buf->buffer_size - 1, f);

    if(read_bytes < 0) {
        printf("Could not read from %s\n", opened_filename);
        exit(1);
    }


    buf->buffer_size = read_bytes;


    // shrink buffer
    // don't!
    //buf->buffer   = realloc(buf->buffer, buf->buffer_size);

    buf->newlines = NULL;
    file_analyze_lines(buf);

    // start on line 1 
    buf->buffer_line_begin = 1;

    buf->dirty = 0;

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

    assert(layout->buf_sz);

    layout->buf     = malloc(layout->buf_sz);
    assert(layout->buf);


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


char footer_message[512] = "footer message";

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
            "\033[%u;%uH%c" // remove last cursor
            "\033[%u;%uH%c" // put new cursor
            "\033[0;0H",    // back to 0:0

            lasty + 1, lastx + 1,
            hidden,

            new->row + sl->top_padding + 1, new->col + LEFT_PADDING + 1,
            CURSOR_CHAR
    );

    fwrite(buf, 1, len, stdout);
}


int line_len(file_buffer_t* fb, int row, char** line_begin) {
    int line_size = fb->newlines[row + 1] - fb->newlines[row];

    char* begin = fb->buffer + fb->newlines[row];

    if(line_size && *begin == '\n') {
        begin++;
        line_size--;
    }

    if(line_begin)
        *line_begin = begin;

    return line_size;
}


void txe_save(file_buffer_t* fb) {

    FILE* file = fopen(opened_filename, "w+");

    if(!file) {
        printf("couldn't save file %s\n", opened_filename);
        exit(1);
    }

    int res = fwrite(fb->buffer, 1, fb->buffer_size, file);
    fclose(file);

    fb->dirty = 0;

    if(res != fb->buffer_size)
        sprintf(footer_message, "could save %s", opened_filename);
    else
        sprintf(footer_message, "saved %s", opened_filename);

}

void txe_quit(file_buffer_t* fb, screen_layout_t* sl) {

    if(!fb->dirty)
        exit(0);


    sprintf(footer_message, "save file before quitting? (y/n)");

    draw_screen(fb, sl);

    int r = getchar();
    if(r < 0)
        exit(1);
    if     (tolower(r) == 'y') {
        txe_save(fb);
        exit(0);
    }
    else if(tolower(r) == 'n')
        exit(0);

}




// draw the whole screen
void draw_screen(file_buffer_t* fb, screen_layout_t* sl) {

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
            char* line_begin;
            int line_size = line_len(fb, l, &line_begin);


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

    //int cursor_line_begin = (cur->row + sl->top_padding) * sl->cols;
    //int cursor_pos        = cursor_line_begin + cur->col + LEFT_PADDING;

    //screen_begin[cursor_pos] = 'X';

    memcpy(curline_begin, cursor_reset_seq, sizeof(cursor_reset_seq));


    


    //size_t len = buf_end - buf_begin;
    // draw the buffer
    fwrite(sl->buf, 1, strlen(sl->buf), stdout);
    //printf("_");
    // print cursor
    //printf("\x1b[%u;%uHX", cur->row + sl->top_padding + 1, cur->col + LEFT_PADDING + 1);
    fflush(stdout);

}



static file_buffer_t  * _fb = NULL;
static screen_layout_t* _sl = NULL;

void sigterm(int n) {

    static int recursive = 0;

    if(recursive) {
        return;
    }
    recursive = 1;

    if(_fb && _sl)
        txe_quit(_fb, _sl);

    recursive = 0;
}


void cleanup(void) {
    printf("\x0c");
}



int main(int argc, char** argv) {

    signal(SIGINT, sigterm);
    signal(SIGTERM, sigterm);



    FILE* file = parse_args(argc, argv);
    assert(file);

    screen_size = load_screen_size();
    assert(screen_size.width > 0);
    assert(screen_size.height > 0);


    file_buffer_t* fb = load_file(file);


    screen_layout_t* sl = init_screen_layout();
    
    // save for signals
    _fb = fb;
    _sl = sl;

    cursor_t cur = {
        .file_offset = 0,
        .row = 0,
        .col = 0,
    };

    cursor_t old_cur;

    atexit(cleanup);


    draw_screen(fb, sl);

    while(1) {
        int c = getc(stdin);

        old_cur = cur;

        int modified = read_input(c, fb, &cur, sl);


        switch(modified) {
            case 1: 
                draw_screen(fb, sl);
                
                update_cursor(fb, sl, &cur, &old_cur);
                break;
            default:
                update_cursor(fb, sl, &cur, &old_cur);
        }
        
    }
}
