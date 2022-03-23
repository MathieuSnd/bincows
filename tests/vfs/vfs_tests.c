#include <stdio.h>
#include <stdlib.h>


#ifdef NDEBUG
#define assert(x) do { (void)sizeof(x);} while (0)
#else
#include <assert.h>
#endif

#include <fs/vfs.h>
//#include <assert.h>
#include <lib/string.h>
#include <lib/panic.h>
#include <lib/logging.h>
#include <lib/dump.h>
#include <acpi/power.h>


#include "../tests.h"

void disk_tb_install(const char* path);

static void log_tree(const char *path, int level)
{

    struct DIR *dir = vfs_opendir(path);

    struct dirent *dirent;

    assert(dir);

    while ((dirent = vfs_readdir(dir)))
    {
        for (int i = 0; i < level + 1; i++)
            puts("-");

        printf(" %d - %s (size %u) \n", dirent->type, dirent->name, dirent->file_size);

        if (dirent->type == DT_DIR)
        {
            char *pathb = malloc(strlen(path) + strlen(dirent->name) + 2);

            strcpy(pathb, path);
            strcat(pathb, "/");
            strcat(pathb, dirent->name);

            log_tree(pathb, level + 1);

            free(pathb);
        }
    }

    vfs_closedir(dir);
}

static inline
void test_print_file(int size) {
    file_handle_t* f = vfs_open_file("/fs/boot/limine.cfg");
    assert(f);
    char buf[size+1];
    int read = 0;

    while((read = vfs_read_file(buf, 1, size, f)) != 0) {
        buf[read] = 0;
        printf("%s", buf);
    }

    vfs_close_file(f);
}

// read & seek test function
static inline
void test_write(void) {
    file_handle_t* f = vfs_open_file("/fs/file.dat");
    assert(f);
    int r = vfs_seek_file(f, 0, SEEK_END);
    assert(!r);
    log_warn("FILE SIZE = %u", vfs_tell_file(f));

    size_t seek = 0;//234567;

    vfs_seek_file(f, seek, SEEK_SET);


    uint8_t buf[512] = {0xff};
    for(int i = 0; i < 512; i++)
        buf[i] = 0xff;

    log_warn("WRITE = %u", vfs_write_file(buf, 512, 1, f));

    vfs_seek_file(f, seek, SEEK_SET);

    char rd[1025];
    memset(rd, 0x69, 1024);

    size_t read = vfs_read_file(rd, 1024, 1, f);
    assert(read == 1);
    rd[1024] = 0;
    printf("READ: %s\n\n", rd);
    printf("READ: %c\n\n", *rd);
    dump(rd, 1024, 32, DUMP_HEX8);
    vfs_close_file(f);
}


void read_seek_big_file(size_t SIZE) {
    assert(SIZE < 1024 * 1024);

    file_handle_t* f = vfs_open_file("/fs/file.dat");
    
    vfs_seek_file(f, 0, SEEK_END);

    assert(vfs_tell_file(f) == 1024*1024);
    
    vfs_seek_file(f, 0, SEEK_SET);


    char* buf = malloc(SIZE+1); 
    int read = 0;


    int x = 531;



    for(int i = 0; i < 200; i++)
    {

        x = (x * 411 + 1431) % (1024 * 1024 / 4 - SIZE / 4);

        size_t y = x*4;
        //x < 1024 * 1024 / 4 - SIZE
        ///y < 1024 * 1024 - SIZE*4 < 1024**2


        vfs_seek_file(f, y, SEEK_SET);

        read = vfs_read_file(buf, 1, SIZE, f);
        assert(read);

        buf[read] = 0;

        uint32_t* chunk = (uint32_t*)buf;
        for(unsigned j = 0; j < SIZE / 4; j++) {
            if(chunk[j] != j + x) {
                log_warn("chunk[j]=%x, j + x=%x", chunk[j], j+x);
                panic("no :(");
            }
            assert(chunk[j] == j + x);
        }

        //dump(buf, SIZE, 8, DUMP_DEC32);
    }
    vfs_close_file(f);
}



static inline
void test_file_write_extend(void) {
    file_handle_t* writer = vfs_open_file("/////fs/boot/limine.cfg//");
    file_handle_t* reader = vfs_open_file("/fs//./boot//..//boot/limine.cfg");
//    exit(0);

    assert(writer);

    size_t dsize = 3000;
    uint8_t* buf = malloc(dsize);

    for(int i = 0; i < dsize; i++)
        buf[i] = 0xff;
    
    //vfs_seek_file(writer, 0, SEEK_END);
    vfs_write_file(buf, 1, dsize, writer);
    assert(reader);

    char rdbuf[52];
    int read;
    int i;

    vfs_seek_file(writer, 0, SEEK_SET);


    memset(buf, 0, dsize);

    vfs_read_file(buf, 1, dsize, writer);

    dump(buf, dsize, 32, DUMP_HEX8);
    
    printf("SIZE = %u", vfs_tell_file(reader));

    vfs_close_file(reader);
    vfs_close_file(writer);
}




static inline
void test_disk_overflow(void) {
    file_handle_t* f = vfs_open_file("/////fs/boot/limine.cfg//");

    const int bsize = 8 * 1024 * 1024;  

    const size_t size = bsize * 20;

    uint16_t* buf = malloc(bsize);
    for(int i = 0; i < bsize/2; i++)
        buf[i] = i;

    uint64_t time = clock();

    for(unsigned i = 0; i < size / bsize; i++) {
        log_debug("write %u (%u)", i * bsize, clock() - time);
        time = clock();
        
        assert(vfs_write_file(buf, bsize, 1, f) == 1); 
    }
    //assert(0);

// check
    //read
    vfs_close_file(f);
    
    f = vfs_open_file("/////fs/boot/limine.cfg//");

    time = clock();
    int rsize = bsize;
    int i = 0;
    while(vfs_read_file(buf, rsize, 1, f) == 1) {
        int begin = i++ * rsize;
        log_debug("read %u (%u)", begin, clock() - time);
        time = clock();

        for(int j = 0; j < (rsize)/2; j++)
            assert(buf[j] == (j & 0xffff));
        
        memset(buf, 0xff, rsize);
            
    }
    vfs_close_file(f);
    free(buf);
}


void test_open(void) {
    vfs_open_file("");
    vfs_open_file("c");
    vfs_opendir("");
    vfs_opendir("c");
}


#ifndef DISKFILE
#define DISKFILE "disk.bin"
#warning DISKFILE undefined
#endif


void remove_all_drivers();
void free_all_devices();

void test_vfs() {
    


    vfs_init();
    atshutdown(remove_all_drivers);
    atshutdown(free_all_devices);
    atshutdown(gpt_cleanup);

    printf("DISKFILE: %s\n", DISKFILE);
    disk_tb_install(DISKFILE);
    disk_part_t* part = search_partition("Bincows");
    assert(part);

    vfs_mount(part, "/fs");

    //TEST(test_disk_overflow());
    //TEST(test_print_file(441));

    

    //TEST(read_seek_big_file(1));
    //TEST(read_seek_big_file(234));
    //TEST(read_seek_big_file(512));
    //TEST(read_seek_big_file(513));
    //TEST(read_seek_big_file(456523));
    //TEST(read_seek_big_file(145652));
    //TEST(test_write());
    //TEST(test_open());
    TEST(test_disk_overflow());

    //vfs_unmount(part)
    shutdown();

}


void kfree(void* addr) {
    free(addr);
}
void* kmalloc(size_t s) {
    return malloc(s);
}
void* krealloc(void* p, size_t s) {
    return realloc(p,s);
}

int main() {
    //test_vfs();
    //test_vfs();   
    test_vfs();
}
