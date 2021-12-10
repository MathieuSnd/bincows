#pragma once

#include <stdint.h>

void ps2kb_init(void);


struct kbevent {
    enum {
        KEYRELEASED = 0, 
        KEYPRESSED = 1
    } type;
    uint32_t keycode;
    uint32_t scancode;
};

typedef void (* kbevent_handler)(const struct kbevent* kbevent);

void ps2kb_set_event_callback(kbevent_handler h);