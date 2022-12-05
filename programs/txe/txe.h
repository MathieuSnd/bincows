#pragma once

#include <stddef.h>
#include <stdint.h>

typedef 
struct file_buffer {

    // file offset of the buffer beginning
    size_t buffer_begin;

    // file line number of the buffer beginning
    size_t buffer_line_begin;

    char* buffer;
    size_t buffer_size;

    // array offsets of every newline in the buffer
    uint32_t* newlines;

    size_t newlines_count;

    int dirty;
} file_buffer_t;


int line_len(file_buffer_t* fb, int row, char** line_begin);


typedef struct {
    size_t file_offset;
    int row;
    int col;
} cursor_t;

void file_analyze_lines(file_buffer_t* buf);

#define LEFT_PADDING 6
#define SPACES_PER_TAB 4

typedef
struct screen_layout {
    int top_padding;
    int bottom_padding;
    int lines;
    int cols;
    
    char* buf;
    int   buf_sz;
} screen_layout_t;


// from txe.c
void file_analyze_lines(file_buffer_t* buf);


// from input.c
// returns 1 iif the screen should be redrawn
int read_input(int c, file_buffer_t* fb, cursor_t* cur, screen_layout_t* sl);

void draw_screen(file_buffer_t* fb, screen_layout_t* sl);


void txe_save(file_buffer_t* fb);
void txe_quit(file_buffer_t* fb, screen_layout_t* sl);