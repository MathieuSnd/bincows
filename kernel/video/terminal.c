#include <stddef.h>
#include <stdbool.h>
#include "terminal.h"
#include "../debug/assert.h"
#include "video.h"
#include "../memory/kalloc.h"
#include "../klib/string.h"

#define TAB_SPACE 6

static void write_string(const char *string, size_t length);
static struct Char make_Char(char c);

static terminal_handler_t terminal_handler = NULL;
static uint32_t terminal_foreground = 0xa0a0a0;
static uint32_t terminal_background = 0x00;

// need to redraw the entire terminal
static bool need_refresh = false;

struct Char {
    uint32_t fg_color: 24;
    char c;
    uint32_t bg_color;
};

#define CHARMAP_W 192
#define CHARMAP_H 256

#define FONTWIDTH (CHARMAP_W / 16)
#define FONTHEIGHT (CHARMAP_H / 16)
#define INTERLINE 4
#define LINE_HEIGHT (FONTHEIGHT + INTERLINE)


static Image* charmap = NULL;
static struct Char* buffer = NULL;

static uint16_t ncols, nlines;
static uint16_t cur_col, cur_line;

static uint32_t current_fgcolor = 0xffffff;
static uint32_t current_bgcolor = 0;



// "int" but designates a binary file 
extern int _binary_charmap_bmp;


void setup_terminal(void) {
    assert(charmap == NULL);
    
    charmap = loadBMP(&_binary_charmap_bmp);

    assert(charmap      != NULL);
    assert(charmap->bpp == 32);
    assert(charmap->w   == CHARMAP_W);
    assert(charmap->h   == CHARMAP_H);
    
    const Image* screenImage = getScreenImage();
    
// dynamicly create the terminal
// with right size
    ncols  = screenImage->w / FONTWIDTH;
    nlines = screenImage->h / LINE_HEIGHT;

    size_t buffer_len = screenImage->w * screenImage->h;
                                
    
    buffer = kmalloc(buffer_len * sizeof(struct Char));

    struct Char* ptr = buffer;
    for(;buffer_len > 0; --buffer_len)
        *(ptr++) = make_Char(0);


    cur_col = 0;
    cur_line= 0;


    set_terminal_handler(write_string);

}

/*
// return a ptr to the actual char at pos 
static struct Char* get_Char_at(int l, int c) {
    return & buffer [ c + l * ncols ];
}
*/

static void scroll(int lines) {
    need_refresh = true;
}

static void next_line(void) {
    cur_col = 0;
    cur_line++;
    if(cur_line >= nlines) {
        cur_line = nlines;

        scroll(1);
    }
}

// create the char struct
static struct Char make_Char(char c) {
    return (struct Char) {
        current_fgcolor,
        c,
        current_bgcolor
    };
}

static void advance_cursor(char c) {
    switch(c) {
        // any character
    default:
        buffer[ncols * cur_line + cur_col] = make_Char(c);

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

// emplace the char in the buffer, dosn't draw anything
static void emplace_char(char c) {
    advance_cursor(c); 
}

static void print_char(const struct Char* c, int line, int col) {
    uint16_t c_x = c->c % 16,
             c_y = c->c / 16;

    Pos srcpos = {
        FONTWIDTH  * c_x ,
        FONTHEIGHT * c_y,
    };
    Rect dstrect = {
        .x = col  * FONTWIDTH, 
        .y = line * LINE_HEIGHT,

        .w = FONTWIDTH,
        .h = FONTHEIGHT,
    };
    imageDraw(charmap, &srcpos, &dstrect);

    
    Rect interlineRect = {
        .x = dstrect.x, 
        .y = dstrect.y + FONTHEIGHT,

        .w = FONTWIDTH,
        .h = INTERLINE,
    };

    imageFillRect(c->bg_color, &interlineRect);
    
}

static void flush_screen(void) {
//    if(need_refresh)

//    imageDraw(charmap, NULL, NULL);

    const struct Char* curr = buffer;
    for(size_t l = 0; l < nlines; l++) {
        for(size_t c = 0; c < ncols; c++) {
            print_char(curr++, l, c);
        }
    }
}


static void write_string(const char *string, size_t length) {
    
    for(;length>0;--length) {
        char c = *string++;
        
        if(!c)
            break;
        emplace_char(c);

    }
    
    flush_screen();

    //imageDraw(charmap, NULL, NULL);

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

