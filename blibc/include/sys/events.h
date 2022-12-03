#ifndef _SYS_EVENTS
#define _SYS_EVENTS


// @todo move to /dev/ps2
#define SYS_EVENT_FILE "/dev/ps2kb"

enum sys_event_type {
    KEYPRESSED  = 1,
    KEYRELEASED = 2,
    KEYREPEAT   = 3,

    MOUSE_MOVE  = 5,
};


struct sys_event {
    enum sys_event_type type;

    int keycode;
    int scancode;

    // 0-terminated sequence
    char unix_sequence[8];
};

#endif//_SYS_EVENTS