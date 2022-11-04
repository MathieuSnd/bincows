#include "fs/vfs.h"
#include "lib/string.h"
#include "lib/panic.h"
#include "lib/assert.h"
#include "lib/logging.h"
#include "lib/registers.h"
#include "lib/sprintf.h"
#include "int/idt.h"
#include "int/apic.h"
#include "memory/heap.h"
#include "memory/paging.h"


static
void test_disk_write(void) {
    file_handle_t* f = vfs_open_file("//////home//testfile//", VFS_WRITE);

    if(!f) {
        log_debug("cannot open /home/testfile in write mode");
        panic("test failed");
    }

    const char* s = "Hello, world!";

    vfs_write_file(s, strlen(s), f);
// check
    //read
    vfs_close_file(f);
    
    f = vfs_open_file("//////home//testfile//", VFS_READ);

    if(!f) {
        log_debug("cannot open /home/testfile in read mode");
        panic("test failed");
    }


    // read and print file
    char buf[2048];
    size_t read = vfs_read_file(buf, 2048, f);

    if(strncmp(s, buf, strlen(s))) {
        log_debug("read %u bytes:", read);
        printf("%s", buf);
        log_debug("which does not begin with %s", s);
    }

    vfs_close_file(f);
}



static
void test_disk_overflow(void) {
    file_handle_t* f = vfs_open_file("//////home//testfile//", VFS_WRITE);


    if(!f) {
        log_debug("cannot open /home/testfile in write mode");
        panic("test failed");
    }


    const int bsize = 1024 * 1024 * 8;

    const size_t size = 1024*1024*64;

    uint8_t* buf = malloc(bsize);
    for(int i = 0; i < bsize; i++)
        buf[i] = i;

    uint64_t time = clock_ns();

    for(unsigned i = 0; i < size / bsize; i++) {
        log_debug("write %u (%u)", i * bsize, (clock_ns() - time) / 1000000);
        time = clock_ns();
        
        size_t r = vfs_write_file(buf, bsize, f);
        assert(r == 1); 
    }

// check
    //read
    vfs_close_file(f);
    
    f = vfs_open_file("//////home//testfile//", VFS_READ);


    if(!f) {
        log_debug("cannot open /home/testfile in read mode");
        panic("test failed");
    }


    time = clock_ns();
    int rsize = (int) bsize;
    int i = 0;
    while(vfs_read_file(buf, rsize, f) == (size_t)rsize) {
        int begin = i++ * rsize;
        log_debug("read %u (%u)", begin, (clock_ns() - time) / 1000000);
        time = clock_ns();

        for(int j = begin; j < begin + rsize; j++)
            assert(buf[j - begin] == (j & 0xff));
            
    }
    vfs_close_file(f);
    free(buf);
}





static 
void test_disk_read(void) {
    file_handle_t* fd = vfs_open_file("//////bin/sh//", VFS_READ);


    if(!fd) {
        log_debug("cannot open /home/sh in read mode");
        panic("test failed");
    }

    
    const int size = vfs_seek_file(fd, 0, SEEK_END);

    if(size <= 0) {
        log_debug("vfs_seek_file returned %d", size);
        panic("test failed");
    }

    int pos = vfs_seek_file(fd, 0, SEEK_SET);

    if(pos != 0) {
        log_debug("2nd vfs_seek_file returned %d", pos);
        panic("test failed");
    }




    vfs_close_file(fd);


    // open 30 times the same file and check its checksum

    int checksum = -1;

    for(int i = 0; i < 30; i++) {

        // alloc user memory
        uint64_t pm = alloc_user_page_map();

        // map user memory
        set_user_page_map(pm);

        uint8_t* buf = malloc(size);
        memset(buf, 0, size);        

        fd = vfs_open_file("/////bin/sh//", VFS_READ);



        int r = vfs_read_file(buf, size, fd);

        // I observed that putting delay here
        // can induce errors
        for(int i = 0; i < 8; i++)
            asm("hlt");


        vfs_close_file(fd);


        assert(r == size);

        int sum = memsum(buf, size);
        
        // init
        if(i == 0) {
            checksum = sum;
            log_warn("test optained /bin/sh sum=%u, please check this value", sum);
        }
        
        if(sum != checksum) {
            log_debug(
                "test iteration %u: got sum of %u instead of %u for previous iterations", 
                i, sum, checksum
            );
            panic("test failed");
        }

        unmap_user();
        free_user_page_map(pm);
        free(buf);

        printf(".");
    }
}



void run_tests(void) {
    
    log_info("test disk read");
    test_disk_read();
    log_info("PASSED");

    log_info("test disk write");
    test_disk_write();
    log_info("PASSED");


    log_info("test disk overflow");
    test_disk_overflow();
    log_info("PASSED");
    
}