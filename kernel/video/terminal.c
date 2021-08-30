#include <stddef.h>
#include "terminal.h"
#include "../debug/assert.h"
#include "video.h"
#include "../memory/kalloc.h"
#include "../klib/string.h"

#define TAB_SPACE 6


terminal_handler_t terminal_handler = NULL;
uint32_t terminal_foreground = 0xa0a0a0;
uint32_t terminal_background = 0x00;

struct Char {
    uint32_t fg_color: 24;
    char c;
    uint32_t bg_color;
};

#define CHARSIZE 16

Image* charmap = NULL;
struct Char* buffer = NULL;

int ncols, nlines;
int cur_col, cur_line;



// "int" but designates a binary file 
extern int _binary____resources_bmp_charmap_bmp_start;


void setup_terminal(void) {
    assert(charmap != NULL);
    
    charmap = loadBMP(&_binary____resources_bmp_charmap_bmp_start);

    assert(charmap != NULL);
    assert(charmap->bpp == 32);
    assert(charmap->w   == 256);
    assert(charmap->h   == 256);
    
    const Image* screenImage = getScreenImage();
    
    ncols  = screenImage->w / CHARSIZE;
    nlines = screenImage->h / CHARSIZE;

    size_t buffer_size = screenImage->w * screenImage->h * 
                                sizeof(struct Char);
    buffer = kmalloc(buffer_size);

    memset(buffer, 0, buffer_size);

    cur_col = 0;
    cur_line= 0;

}

static void scroll(int lines) {

}

static void next_line(void) {
    cur_col = 0;
    cur_line++;
    if(cur_line >= nlines) {
        cur_line = nlines;

        scroll(1);
    }
}

static void advance_cursor(char c) {
    switch(c) {
    default:
        cur_col += 1;

        if(cur_col >= ncols)
            next_line();
        break;
    
    case '\n':
        next_line();
        break;
    
    case '\t':
        cur_col = (cur_col / TAB_SPACE) * TAB_SPACE;
        
        if(cur_col >= ncols)
            next_line();
        break;

    case '\r':
        cur_col = 0;
        break;
    }
}

void printchar(char c) {

    advance_cursor(c); 
}






terminal_handler_t get_terminal_handler(void) {
    return terminal_handler;
}


void set_terminal_handler(terminal_handler_t h) {
    terminal_handler = h;
}

void terminal_set_colors(uint32_t foreground, uint32_t background) {
    terminal_foreground = foreground;
    terminal_background = background;
}

