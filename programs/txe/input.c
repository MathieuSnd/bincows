#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "txe.h"

static
int offset_to_line(file_buffer_t* fb, size_t off) {
    int l = 0;

    while(fb->newlines[l] < off) {
        l++;
        assert(l < fb->newlines_count);
    }

    return l - 1;
}


void up_arrow   (file_buffer_t* fb, cursor_t* cur, screen_layout_t* sl) {
    if(cur->row > 0)
        cur->row--;

}

void down_arrow (file_buffer_t* fb, cursor_t* cur, screen_layout_t* sl) {
    if(cur->row < sl->lines - sl->bottom_padding - sl->top_padding
        && cur->row < fb->newlines_count - 1)
        cur->row++;
}


void right_arrow(file_buffer_t* fb, cursor_t* cur, screen_layout_t* sl) {
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


void left_arrow (file_buffer_t* fb, cursor_t* cur, screen_layout_t* sl) {
    if(cur->col > 0)
        cur->col--;
}


void emplace_char(file_buffer_t* fb, cursor_t* cur, screen_layout_t* sl, int ch) {
    
}




void read_input(int c, file_buffer_t* fb, cursor_t* cur, screen_layout_t* sl) {
    if(c == EOF)
        exit(1);

    static int seq = 0;
    //exit(1);

    if(c == '\x1b') {
        seq = 1;
        return;
    }

    if(seq) {
        if(seq == 1) {
            fb->buffer[0] = c;
        }
        if(c == '[' && seq == 1) {
            seq = 2;
        }

        else if(seq == 2) {
            
            switch(c) {
                case 'A': // up arrow
                    up_arrow   (fb, cur, sl);
                    break;
                case 'B': // down arrow
                    down_arrow (fb, cur, sl);
                    break;
                case 'C': // right arrow
                    right_arrow(fb, cur, sl);
                    break;
                case 'D': // left arrow
                    left_arrow (fb, cur, sl);
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

        return;
    }
    

    switch (c)
    {
    default:
        emplace_char(fb, cur, sl, c);
        // write key
        break;
    }    
}
