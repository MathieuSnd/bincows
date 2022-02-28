#include <stdio.h>
#include <fs/vfs.h>
#include <lib/assert.h>
#include <lib/panic.h>
#include <lib/logging.h>
#include <lib/dump.h>
#include <stdlib.h>

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


// read & seek test function
static inline
void test_file_read_seek(void) {
file_handle_t* f = vfs_open_file("/fs/file.dat");
    assert(f);

    assert(!vfs_seek_file(f, 512, SEEK_SET));
    log_warn("FILE SIZE = %u", vfs_tell_file(f));

    vfs_seek_file(f, 50, SEEK_CUR);

    char rd[513];
    assert(vfs_read_file(rd, 512, 1, f) == 1);
    rd[512] = 0;
    printf("READ: %512s", rd);
    dump(rd, 512, 32, DUMP_HEX8);

    vfs_seek_file(f, 50, SEEK_CUR);


    char buf[512] = 
        "Une balle pourtant, mieux ajustée ou plus traître que les autres, finit par atteindre "
        "l'enfant feu follet. On vit Gavroche chanceler, puis il s'affaissa. Toute la barricade "
        "poussa un cri ; mais il y avait de l'Antée dans ce pygmée ; pour le gamin toucher le "
        "pavé, c'est comme pour le géant toucher la terre ; Gavroche n'était tombé que pour se "
        "redresser ; il resta assis sur son séant, un long filet de sang rayait son visage,"
        "il éleva ses deux bras en l'air, regarda du côté d'où était venu le coup, et se mit à";
    vfs_write_file(buf, 512, 1, f);
    vfs_close_file(f);
    return;
#define SIZE 512
/*

    vfs_seek_file(f, 0, SEEK_END);
    
    vfs_seek_file(f, 0, SEEK_SET);



    char* buf = malloc(SIZE+1); 
    int read = 0;


    int x = 531;

    for(int i = 0; i < 200; i++)
    {

        x = (x * 411 + 1431) % (1024 * 1024 / 4 - SIZE);

        size_t y = x*4;

        log_info("test %u", y);

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
    */
}



int main() {
    vfs_init();
    disk_tb_install("../../disk.bin");
    disk_part_t* part = search_partition("Bincows");
    assert(part);
    printf("MOUNT");
    vfs_mount(part, "/fs");


    test_file_read_seek();
}
