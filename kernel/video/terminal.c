#include <stddef.h>
#include <stdbool.h>
#include "terminal.h"
#include "../debug/assert.h"
#include "video.h"
#include "../klib/string.h"

#define TAB_SPACE 6


struct Char {
    uint32_t fg_color: 24;
    char c;
    uint32_t bg_color;
};


static void write_string(const char *string, size_t length);
static struct Char make_Char(char c);
static void print_char(const struct Char* restrict c, int line, int col);
static void flush_screen(void);

static terminal_handler_t terminal_handler = NULL;

// need to redraw the entire terminal
static bool need_refresh = false;


void default_terminal_handler(const char* s, size_t l) {
    (void) (s + l);
    // empty handler by default,
    // make sure not to execute the address 0 :)
}

#define NCOLS_MAX 133
#define TERMINAL_LINES_MAX 75

static Image* charmap = NULL;
static struct Char buffer[NCOLS_MAX * TERMINAL_LINES_MAX * TERMINAL_N_PAGES];

static uint16_t ncols, nlines;
static uint16_t term_nlines;
static uint16_t first_line = 0;
static uint16_t cur_col, cur_line;

static uint32_t current_fgcolor = 0xa0a0a0;
static uint32_t current_bgcolor = 0;



// "int" but designates a binary file 
extern int _binary_charmap_bmp;


void setup_terminal(void) {
    assert(charmap == NULL);
    
    charmap = loadBMP_24b_1b(&_binary_charmap_bmp);
    
    
    assert(charmap      != NULL);
    assert(charmap->bpp   == 1);
    assert(charmap->pitch == 1);
    assert(charmap->w   == TERMINAL_CHARMAP_W);
    assert(charmap->h   == TERMINAL_CHARMAP_H);
    
    const Image* screenImage = getScreenImage();
    
// dynamicly create the terminal
// with right size

    ncols       = screenImage->w / TERMINAL_FONTWIDTH;
    term_nlines = (screenImage->h / TERMINAL_LINE_HEIGHT) - 3;

                                
    if(ncols > NCOLS_MAX)
        ncols = NCOLS_MAX;
    if(term_nlines > TERMINAL_LINES_MAX)
        term_nlines = TERMINAL_LINES_MAX;

    nlines      = TERMINAL_N_PAGES * term_nlines;



    set_terminal_handler(write_string);
}


void terminal_clear(void) {
    cur_col    = 0;
    cur_line   = 0;
    first_line = 0;
    
    size_t buffer_len = nlines * ncols;

    struct Char* ptr = buffer;
    for(;buffer_len > 0; --buffer_len)
        *(ptr++) = make_Char(0);
    
    flush_screen();
}


void set_terminal_fgcolor(uint32_t c) {
    current_fgcolor = c;
}
void set_terminal_bgcolor(uint32_t c) {
    current_bgcolor = c;
}

/*
// return a ptr to the actual char at pos 
static struct Char* get_Char_at(int l, int c) {
    return & buffer [ c + l * ncols ];
}
*/

static void move_buffer(int lines) {
    need_refresh = true;

    if(lines > 0) {// scroll backward
        size_t bytes = ncols * lines;
        size_t buff_size = nlines * ncols;

        memmove(buffer, buffer + bytes, sizeof(struct Char)*(buff_size - bytes));        
    }
}

static void next_line(void) {
    cur_col = 0;
    cur_line++;
    if(cur_line >= nlines) {
        cur_line = nlines-1;

        move_buffer(1);
    }
    else if(cur_line >= first_line + term_nlines) {
        first_line++;
        need_refresh = true;
    }
}

// create the char struct
static struct Char make_Char(char c) {
    return (struct Char) {
        .fg_color = current_fgcolor,
        .c        = c,
        .bg_color = current_bgcolor
    };
}


// emplace the char in the buffer, dosn't draw anything
static void emplace_char(char c) {
    switch(c) {
        // any character
    default:

        buffer[ncols * cur_line + cur_col] = make_Char(c);
        
        const struct Char* ch = &buffer[ncols * cur_line + cur_col];

        cur_col += 1;
        if(!need_refresh)
            print_char(ch, cur_line - first_line, cur_col);

        if(cur_col >= ncols)
            next_line();
        
        break;
    
    case '\n':
        next_line();
        break;
    
    case '\t':
        cur_col = ((cur_col + TAB_SPACE) / TAB_SPACE) * TAB_SPACE;
        
        if(cur_col >= ncols)
            next_line();
        break;

    case '\r':
        cur_col = 0;
        break;
    }
}


static void print_char(const struct Char* restrict c, int line, int col) {
/*
    Pos srcpos = {
        FONTWIDTH  * c_x,
        FONTHEIGHT * c_y,
    };
    Rect dstrect = {
        .x = col  * FONTWIDTH, 
        .y = line * LINE_HEIGHT,

        .w = FONTWIDTH,
        .h = FONTHEIGHT,
    };
*/
    //imageDrawMask(charmap, &srcpos, &dstrect, c->fg_color, c->bg_color);
    //imageLower_draw(charmap, &srcpos, &dstrect);
    //imageLower_blitBinaryMask(
    //    charmap,
    //    FONTWIDTH * c_x,  FONTHEIGHT * c_y,
    //    col  * FONTWIDTH, line * LINE_HEIGHT,
    //    FONTWIDTH, FONTHEIGHT,
    //    c->bg_color, c->fg_color);
    //
    /*
    Rect interlineRect = {
        .x = col  * FONTWIDTH, 
        .y = line * LINE_HEIGHT + FONTHEIGHT,
        .w = FONTWIDTH,
        .h = INTERLINE,
    };
*/
  /*
    blitcharX2(charmap, c->c, c->fg_color, c->bg_color,
                col  * FONTWIDTH * 2, 2 *line * LINE_HEIGHT);
    
*/


    blitchar(charmap, c->c, c->fg_color, c->bg_color,
                col  * TERMINAL_FONTWIDTH, line * TERMINAL_LINE_HEIGHT);


    //imageDraw(charmap, NULL, NULL);

    //imageFillRect(c->bg_color, &interlineRect);
    
}

static void flush_screen(void) {
        // begins at the terminal's first line
    const struct Char* curr = buffer + first_line * ncols;

    for(size_t l = 0; l < term_nlines; l++) {
        for(size_t c = 0; c < ncols; c++) {
            print_char(curr++, l, c);
        }
    }
}


static void write_string(const char *string, size_t length) {

    need_refresh = false;

    for(;length>0;--length) {
        char c = *string++;
        
        if(!c)
            break;
        emplace_char(c);

    }
    if(need_refresh)
        flush_screen();
}


terminal_handler_t get_terminal_handler(void) {
    return terminal_handler;
}



void set_terminal_handler(terminal_handler_t h) {
    terminal_handler = h;
}

void terminal_set_colors(uint32_t foreground, uint32_t background) {
    current_fgcolor = foreground;
    current_bgcolor = background;
}
