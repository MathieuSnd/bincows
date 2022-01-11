#pragma once

struct driver;

int nvme_install(struct driver*);

#define NVME_INVALID_IO_QUEUE (~0u)
