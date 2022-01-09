#pragma once

struct driver;

int nvme_install(struct driver*);

#define NVME_INVALID_IO_QUEUE (~0u)

// return the id of the newly created IO 
// queue or NVME_INVALID_IO_QUEUE
unsigned nvme_create_ioqueue(struct driver*);

void nvme_delete_ioqueue(struct driver* this, 
                         unsigned queueid);
