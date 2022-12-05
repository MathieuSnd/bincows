#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "txe.h"

static
int offset_to_line(file_buffer_t* fb, size_t off) {
    int l = 0;

    while(fb->newlines[l] < off) {
        l++;
        assert(l <= fb->newlines_count);
    }

    return l - 1;
}

// debug @todo remove
extern char footer_message[512];


static 
void update_cursor_file_offset(file_buffer_t* fb, cursor_t* cur, screen_layout_t* sl) {
    assert(cur->row < fb->newlines_count);


    char* line_begin;
    int len = line_len(fb, cur->row, &line_begin);
    int line_file_offset = line_begin - fb->buffer;

    assert(line_file_offset >= 0);



    cur->file_offset = line_file_offset + cur->col;
}


void cursor_up   (file_buffer_t* fb, cursor_t* cur, screen_layout_t* sl) {
    if(cur->file_offset == 0)
        return;

    
    if(cur->row > 0)
        cur->row--;
    else {
        assert(cur->col > 0);
        cur->col = 0;
    }

    int len = line_len(fb, cur->row, NULL);

    if(cur->col > len)
        cur->col = len;

    

    update_cursor_file_offset(fb,cur,sl);

}

void cursor_down (file_buffer_t* fb, cursor_t* cur, screen_layout_t* sl) {

    if(cur->row >= fb->newlines_count - 1)
        return;
    
    cur->row++;


    int len = line_len(fb, cur->row, NULL);

    if(cur->col > len)
        cur->col = len;
    
    update_cursor_file_offset(fb,cur,sl);
        
}


void cursor_right(file_buffer_t* fb, cursor_t* cur, screen_layout_t* sl) {
    if(cur->file_offset < fb->buffer_size) {
        cur->file_offset++;
        
        int nline = offset_to_line(fb, cur->file_offset);
        
        if(nline == cur->row) {
            cur->col++;
        }
        else {
            // line border crossed
            cur->col = 0;
            cur->row = nline;
        }
    }
}


void cursor_left (file_buffer_t* fb, cursor_t* cur, screen_layout_t* sl) {
    if(cur->file_offset == 0)
        return;

    cur->file_offset--;
    if(cur->col > 0)
        cur->col--;
    else {
        assert(cur->row > 0);

        cur->row--;
        cur->col = line_len(fb, cur->row, NULL);
    }

    
}


void emplace_char(file_buffer_t* fb, cursor_t* cur, screen_layout_t* sl, int ch) {

    fb->dirty = 1;

    int file_offset = cur->file_offset;


    // 0: rigth, 1: left
    int left;

    sprintf(footer_message, "character %u", ch);
    

    if(ch == '\b' || ch == 127 /* DEL */) {
        if(cur->col == 0 && cur->row == 0)
            return;

        cursor_left(fb, cur, sl);

        
        memmove(
            fb->buffer + file_offset - 1, 
            fb->buffer + file_offset,
            fb->buffer_size - file_offset
        );
        left = 1;
        fb->buffer_size--;
    }
    
    else {
        fb->buffer = realloc(fb->buffer, fb->buffer_size + 1);


        memmove(
            fb->buffer + file_offset + 1, 
            fb->buffer + file_offset,
            fb->buffer_size - file_offset
        );
        fb->buffer[file_offset] = ch;

        fb->buffer_size += 1;


        left = 0;
    }

    file_analyze_lines(fb);

    if(!left)
        cursor_right(fb, cur, sl);

}




int read_input(int c, file_buffer_t* fb, cursor_t* cur, screen_layout_t* sl) {
    if(c == EOF)
        exit(1);

    static int seq = 0;
    //exit(1);

    if(c == '\x1b') {
        seq = 1;
        return 0;
    }

    if(seq) {
        if(c == '[' && seq == 1) {
            seq = 2;
        }

        else if(seq == 2) {
            
            switch(c) {
                case 'A': // up arrow
                    cursor_up   (fb, cur, sl);
                    break;
                case 'B': // down arrow
                    cursor_down (fb, cur, sl);
                    break;
                case 'C': // right arrow
                    cursor_right(fb, cur, sl);
                    break;
                case 'D': // left arrow
                    cursor_left (fb, cur, sl);
                    break;
                default:
                    // unhandled key
                    break;
            }
            

            seq = 0;
        }
        else {
            seq = 0;
        }

        return 0;
    }

    switch (c)
    {
    default:
        emplace_char(fb, cur, sl, c);
        // write key
        break;
    case '\t':
        for(int i = 0; i < SPACES_PER_TAB; i++)
            emplace_char(fb, cur, sl, ' ');
        break;
    case 's' - 'a' + 1: // ^S
        txe_save(fb);
        break;
    case 'x' - 'a' + 1: // ^X
        txe_quit(fb, sl);
        break;
    case '\r': // new line
        emplace_char(fb, cur, sl, '\n');
        break;
    }    
    return 1;
}