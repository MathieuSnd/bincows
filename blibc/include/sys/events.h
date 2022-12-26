#ifndef _SYS_EVENTS
#define _SYS_EVENTS

#include <stdint.h>

// @todo move to /dev/ps2
#define SYS_EVENT_FILE "/dev/ps2kb"

// keys
#define KEY_ESCAPE 1

enum sys_event_type {
    KEYPRESSED  = 1,
    KEYRELEASED = 2,
    KEYREPEAT   = 3,

    MOUSE_MOVE     = 5,
    MOUSE_PRESSED  = 6,
    MOUSE_RELEASED = 7,
};

#define MOUSE_BLEFT   1
#define MOUSE_BRIGHT  2
#define MOUSE_BMIDLLE 3


struct sys_mouse_event {
    int16_t xrel;
    int16_t yrel;

// either MOUSE_BLEFT, MOUSE_BRIGHt OR MOUSE_BMIDLLE
// if the event type is MOUSE_PRESSED or MOUSE_RELEASED

    int button;
};

struct sys_event {
    enum sys_event_type type;

    
    int keycode;
    int scancode;

    // 0-terminated sequence
    char unix_sequence[8];

    // this structure is undefined if 
    // the event type is not one of the following:
    // MOUSE_MOVE, MOUSE_PRESSED, MOUSE_RELEASED
    struct sys_mouse_event mouse;
};


#endif//_SYS_EVENTS