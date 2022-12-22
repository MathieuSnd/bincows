#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#if defined(__BINCOWS__)
#include <terminfo.h>


struct screen_size load_screen_size(void) {
    terminfo_t ti;

    if(terminfo_read(&ti) == -1) {
        fprintf(stderr, "Could not read terminfo\n");
        exit(1);
    }

    return (struct screen_size) {
        .width = ti.cols,
        .height = ti.lines,
    };
}

#elif defined(__linux__)

#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>

static struct termios orig_termios;  /* TERMinal I/O Structure */


/* exit handler for tty reset */
void tty_atexit(void) {
   tcsetattr(STDIN_FILENO,TCSAFLUSH,&orig_termios);
}



struct screen_size load_screen_size(void) {
	struct winsize size;
	ioctl( 0, TIOCGWINSZ, (char *) &size );


    struct termios raw;

    int ttyfd = STDIN_FILENO;

    tcgetattr(ttyfd,&orig_termios);
    atexit(tty_atexit);

    raw = orig_termios;  /* copy original and then modify below */

    /* input modes - clear indicated ones giving: no break, no CR to NL, 
       no parity check, no strip char, no start/stop output (sic) control */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    /* output modes - clear giving: no post processing such as NL to CR+NL */
    raw.c_oflag &= ~(OPOST);

    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);

    /* local modes - clear giving: echoing off, canonical off (no erase with 
       backspace, ^U,...),  no extended functions, no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);

    tcsetattr(ttyfd,TCSAFLUSH,&raw);


    return (struct screen_size) {
        .width = size.ws_col,
        .height = size.ws_row,
    };
}


#else

#error "unsupported target"
#endif