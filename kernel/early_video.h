#pragma once

struct boot_interface;

// return the terminal driver
struct driver* video_init(const struct boot_interface *);
