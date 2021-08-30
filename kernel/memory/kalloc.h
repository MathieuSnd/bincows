#pragma once

#include <stddef.h>

void* kmalloc(size_t size);
void  kfree(void* p);
