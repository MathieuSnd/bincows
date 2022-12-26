#pragma once
/**
 * this file presents the 
 * terminal info interfadce
 * 
 */


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    int cols;
    int lines;
    int stored_lines;

    int cursor_x;
    int cursor_y;
    int first_line;
} terminfo_t;


// read the current terminal info
int terminfo_read(terminfo_t* info);


#ifdef __cplusplus
}
#endif