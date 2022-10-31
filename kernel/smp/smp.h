#pragma once

#include <stddef.h>
#include "../boot/boot_interface.h"

void init_smp(struct boot_interface *);


size_t get_smp_count(void);