#pragma once


// register function to execute
// right before the computer
// shuts down / reboots
void atshutdown(void (*)(void));

#define MAX_SHUTDOWN_FUNCS 128

void shutdown(void);
void reboot(void);