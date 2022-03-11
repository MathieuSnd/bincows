
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <drivers/storage_interface.h>
#include <drivers/driver.h>
#include <drivers/dev.h>
#include <fs/gpt.h>
#include <lib/dump.h>

struct data {
    FILE* f;
    struct storage_interface si;
};

static
void dread(struct driver* this,
                uint64_t lba,
                void*    buf,
                size_t   count
) {
    struct data* data = this->data;
    size_t size = 1 << data->si.lbashift;

    char* sbuf = malloc(size*count);
    fseek(data->f, lba * size, SEEK_SET);
    assert(fread(sbuf, size, count, data->f) == count);
    memcpy(buf, sbuf, size*count);
    free(sbuf);

    printf("READ %u blocks\n", count);
}


static
void dasync_read(struct driver* this,
                uint64_t lba,
                void*    buf,
                size_t   count
) {
    dread(this,lba,buf,count);
}

static void dsync(struct driver* this) {
    printf("SYNC\n");
    (void) this;
}


static
void dwrite(struct driver* this,
                uint64_t    lba,
                const void* buf,
                size_t      count
) {
    struct data* data = this->data;
    size_t size = 1 << data->si.lbashift;

    fseek(data->f, lba * size, SEEK_SET);
    assert(fwrite(buf, size, count, data->f) == count);
}


static
void dremove(driver_t* this) {
    fclose(this->data);
    this->status = DRIVER_STATE_SHUTDOWN;
}

static 
int install(driver_t* this) {
    FILE* dfile = fopen((void*)(this->device+1), "rb+");
    
    assert(dfile);

    struct data* data = this->data = malloc(sizeof(struct data));
    this->data_len = sizeof(struct data);
    this->remove = dremove;
    this->status = DRIVER_STATE_OK;
    this->name = (string_t){"tb disk controller", 0},

    data->si = (struct storage_interface) {
        .capacity   = ftell(dfile),
        .driver     = this,
        .lbashift   = 9, 
        .read       = dread,
        .async_read = dasync_read,
        .sync       = dsync,
        .write      = dwrite,
    };
    data->f = dfile;

    gpt_scan(&data->si);
    
    return 1;
}


void disk_tb_install(const char* path) {
    struct tmp {
        dev_t dev;
        char path[128];
    }* dev = malloc(sizeof(struct tmp));
    
    *dev = (struct tmp) {
        .dev = {
            .name = {.ptr = "vdisk", .freeable=0},
            .type = 0,
            .driver = NULL,
        },
    };
    strcpy(dev->path, path);

    register_dev((void*)dev);

    driver_register_and_install(install, (void*)dev);

}
