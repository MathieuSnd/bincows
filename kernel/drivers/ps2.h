#pragma once

#include <stdint.h>

void ps2_init(void);

// keys
#define ps2_ESCAPE 1

enum ps2_event_type {
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


struct ps2_mouse_event {
    int16_t xrel;
    int16_t yrel;

// either MOUSE_BLEFT, MOUSE_BRIGHt OR MOUSE_BMIDLLE
// if the event type is MOUSE_PRESSED or MOUSE_RELEASED

    int button;
};

struct ps2_event {
    enum ps2_event_type type;

    
    int keycode;
    int scancode;

    // 0-terminated sequence
    char unix_sequence[8];

    // this structure is undefined if 
    // the event type is not one of the following:
    // MOUSE_MOVE, MOUSE_PRESSED, MOUSE_RELEASED
    struct ps2_mouse_event mouse;
};



void ps2_trigger_CPU_reset(void);

// poll untill the specified key is pressed
void ps2_poll_wait_for_key(uint8_t key);


void ps2_register_dev_file(const char* filename);

