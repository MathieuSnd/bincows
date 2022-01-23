#include "fs.h"
#include "fat32/fat32.h"

int try_mount(disk_part_t* part) {
    return fat32_detect(part);
}