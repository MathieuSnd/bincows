#include <stdio.h>
#include <terminfo.h>
#include <unistd.h>
#include <stdlib.h>

void update_cursor(int x, int y) {
    printf("\033[%d;%dH", y, x);

    printf("%c", '\n');
}


int _terminfo_read(terminfo_t* info) {

    int rd = read(1, info, sizeof(terminfo_t));

    printf("rd = %d", rd);

    return (rd == sizeof(terminfo_t)) ? 0 : -1;
}


char* buffer;
int width;
int height;
int cursor_x;
int cursor_y;


char* init_buffer(void) {
    terminfo_t info;
    int r = terminfo_read(&info);

    if (r != 0) {
        printf("error");
        return NULL;
    }

    width = info.cols;
    height = info.lines;

    return buffer = malloc(width * height);
}


void clear_screen(void) {
    printf("\x0c");
}

#define TAB_SPACE 6

void fill_buffer(FILE* file) {
    for(int y = cursor_y; y < height; y++) {
        for(int x = 0; x < width; x++) {
            int c = fgetc(stdin);

            int eol = 0;

            printf("%c",c);

            switch(c) {
                case EOF:
                    return;
                case '\n':
                    for(int i = x; i < width; i++) {
                        buffer[y * width + i] = ' ';
                    }
                    eol = 1;
                    cursor_x = 0;
                    cursor_y++;
                    break;
                case '\t':
                {
                    cursor_x = ((cursor_x + TAB_SPACE) / TAB_SPACE) * TAB_SPACE;
                    
                    if(cursor_x > width) {
                        cursor_x = 0;
                        cursor_y++;
                    }
                }
                    for(int i = x; i < (x + 4) % 4; i++) {
                        buffer[y * width + i] = ' ';
                    }
                    cursor_x += 4;
                    break;
                case '\b':
                    cursor_x--;
                    break;
                default:
                    buffer[y * width + x] = c;
                    cursor_x++;
                    break;
            }
        }
    }
}


int main(int argc, char** argv) {
    FILE* file;

    FILE* uif;
    
    if(argc > 1) {
        file = fopen(argv[1], "r");
        if(!file) {
            printf("could not open file %s", argv[1]);
            return 1;
        }
        uif = stdin;
    }
    else {
        uif  = fopen("/dev/ps2kb", "r");
        file = stdin;
    }


    init_buffer();
    clear_screen();

    cursor_x = 0;
    cursor_y = 0;

    fill_buffer(file);
    
    printf("DONE.\n");

    return 0;
}