#include "../../lib/assert.h"
#include "../../lib/logging.h"
#include "../../lib/dump.h"
#include "../../lib/math.h"
#include "../../lib/string.h"
#include "../../lib/sprintf.h"
#include "../../lib/utf16le.h"
#include "../../memory/heap.h"

#include "fat32.h"
#include "specs.h"

#define FAT_CACHE_MAX_ENTRIES 64

typedef struct block_cache_entry {
    uint64_t lba;
    char buf[];
    // buf dimension is exactly one sector
} block_cache_entry_t;


typedef struct block_cache {
    uint32_t n_entries;
    uint32_t buffer_head;
    block_cache_entry_t entries[FAT_CACHE_MAX_ENTRIES];
} block_cache_t;

typedef struct {
    uint32_t root_cluster;
    uint32_t clusters_size;
    uint32_t tables_count;
    uint32_t fat_begin;
    uint32_t data_begin;
    uint32_t root_sector;
    uint32_t data_clusters;


    block_cache_t fat_cache;
} fat32_privates_t;




static 
void* read(disk_part_t* part, uint64_t lba, void* buf, size_t count) {
    lba += part->begin;


    assert(lba >= part->begin && lba <= part->end);

    part->interface->read(part->interface->driver, lba, buf, count);

    return buf;
}


static inline
void write(disk_part_t* part, uint64_t lba, void* buf, size_t count) {
    lba += part->begin;

    assert(lba >= part->begin && lba <= part->end);

    part->interface->write(part->interface->driver, lba, buf, count);

}


static unsigned __attribute__((pure)) block_size(disk_part_t* part) {
    return 1 << part->interface->lbashift;
}


static uint64_t cluster_begin(uint32_t cluster, fat32_privates_t* fp) {
    return ((cluster - 2) * fp->clusters_size) + fp->data_begin;
}


/**
 * @brief access cache data
 * 
 * @param cache the cache
 * @param lba  the lba
 * @return void* cache content
 */
static 
void* cache_access(block_cache_t* restrict cache, uint64_t lba) {
    
    // the cache is circular: FIFO
    // we first search in most recent entries
    unsigned id = cache->buffer_head;
    unsigned n_entries = cache->n_entries;

    for(unsigned i = 0; i < n_entries; i++) {
        if(cache->entries[id].lba == lba) 
            return cache->entries[id].buf;

        if(id == 0)
            id = FAT_CACHE_MAX_ENTRIES;
        id--;
    }

    return NULL;
}


/**
 * @brief add a fat cache entry
 * 
 * @param cache cache structure
 * @param lba entry lba
 * @return void* the buffer to fill
 */
static
void* add_fat_cache_entry(block_cache_t* cache, uint64_t lba) {
    if(cache->n_entries < FAT_CACHE_MAX_ENTRIES)
        cache->n_entries++;
    
    void* buf = cache->entries[cache->buffer_head].buf;
    cache->entries[cache->buffer_head].lba = lba;
    
    cache->buffer_head++;
    if(cache->buffer_head == FAT_CACHE_MAX_ENTRIES)
        cache->buffer_head = 0;

    return buf;
}


/**
 * @brief fetch a fat sector, using cache 
 * the cache handles the allocation/deallocation
 * of its memory.
 * 
 * @param part the partition
 * @param cache the cache structure 
 * @param lba the lba of the fat sector
 * @return void* memory block containing
 * the sector content.
 */
static
void* fetch_fat_sector(disk_part_t* part, block_cache_t* cache, uint32_t lba) {
    void* buf = cache_access(cache, lba);
    
    // cache hit
    if(buf)
        return buf;
    
    // cache miss

    buf = add_fat_cache_entry(cache, lba);
    
    read(part, lba, buf, 1);


    return buf;
}

/**
 * @brief read a FAT entry
 * 
 * @param cluster the entry cluster
 * @param last output: non null if this is the last entry of a file
 * @return uint32_t a 28bit block offset for the entry
 */
static 
uint32_t readFAT(disk_part_t* part, 
                 fat32_privates_t* pr, 
                 uint64_t cluster, 
                 int* last
    ) {
    uint32_t offset = cluster * 4;
    uint64_t lba = pr->fat_begin + offset / block_size(part);
    uint32_t sector_offset = offset % block_size(part);
    
    void* sector = fetch_fat_sector(part, &pr->fat_cache, lba);


    uint32_t ent = *(uint32_t *)(sector + sector_offset);

    assert(ent != 0);
    assert(ent != 1);

    if(ent >= 0x0FFFFFF8) // last entry of the file
        *last = 1;
    return ent & 0x0FFFFFFF;
    //0x0FFFFFF8
}


static 
uint32_t fat_attr2fs_type(uint32_t attr) {
    uint32_t ret = 0;

    if(attr & FAT32_DIRECTORY)
        ret |= DT_DIR;
    else
        ret |= DT_REG; // file

    return ret;
}



static void handle_long_filename_entry(fat_long_filename_t* entry, char* name_buf) {

    fat_long_filename_t* e = (fat_long_filename_t*)entry;
    int order = (e->order & 0x0f) - 1;

    uint16_t wchars[] = {
        e->chars0[0],
        e->chars0[1],
        e->chars0[2],
        e->chars0[3],
        e->chars0[4],
        e->chars1[0],
        e->chars1[1],
        e->chars1[2],
        e->chars1[3],
        e->chars1[4],
        e->chars1[5],
        e->chars2[0],
        e->chars2[1],
    };
    char longname[13];
    
    utf16le2ascii(longname, wchars, 13);
    // long filename entry
    memcpy(name_buf + 13 * order, longname, 13);

}

/**
 * @brief 
 * 
 * @param part the partition
 * @param dir the directory entry of the parent dir
 * @param size (output) the number of elements
 * @return dirent_t* malloc-ed memory block containing
 * the list of dir entries
 */
static 
dirent_t* read_dir(
            disk_part_t* restrict part, 
            fat32_privates_t* restrict pr, 
            dirent_t* dir, 
            int* n_entries
) {
    assert(dir->type == DT_DIR);
    
    unsigned bufsize = block_size(part) * pr->clusters_size;
    uint8_t* buf = malloc(bufsize);

    int end = 0;
    uint32_t cluster = dir->cluster;


    dirent_t* entries = NULL;

    // current output entry id
    int j = 0;

    while(!end) {
        entries = realloc(entries, 
                    sizeof(dirent_t) * (j + bufsize / sizeof(fat_dir_t)));

        read(part, cluster_begin(cluster, pr), buf, pr->clusters_size);


        // for long name entries
        int long_entry = 0;


        for(unsigned i = 0; i < bufsize / sizeof(fat_dir_t); i++) {
            fat_dir_t* dir = (fat_dir_t*)buf + i;

            if(dir->name[0] == 0) {
                end = 1;
                break;
            }
            
            if((uint8_t)dir->name[1] == 0xE5)
                continue;


            if(dir->attr == FAT32_LFN) {
                handle_long_filename_entry(dir, entries[j].name);
                long_entry = 1;
                continue;
            }

            // short entry
            if(!long_entry) {
                strncpy(entries[j].name, dir->name, 8);
                char* ptr = entries[j].name+7;
                
                while(*ptr == ' ')
                    ptr--;
                
                if(dir->name[8] != ' ') {
                    // extention
                    *(++ptr) = '.';

                    strncpy(++ptr, dir->name+8, 3);
                    
                    ptr += 3;
                    while(*ptr == ' ')
                        *(ptr--) = '\0';   
                }
                else // no extention
                    *(ptr+1) = '\0';
            }
            else
            //    log_info("issec %u %s",j, entries[j].name);
                entries[j].name[255] = 0;
            // make sure it's null-terminated

            entries[j].type    = fat_attr2fs_type(dir->attr);
            entries[j].cluster = dir->cluster_low | 
                      ((uint32_t)dir->cluster_high << 16);
            
            entries[j].file_size = dir->file_size;
            entries[j].children = NULL;

            j++;

            long_entry = 0;
        }

        if(end)
            break;        

        cluster = readFAT(part, pr, cluster, &end);
    }

    // we took too much memory for sure
    entries = realloc(entries, sizeof(dirent_t) * j);
    dir->children = entries;
    *n_entries = j;
    return entries;
}


void log_tree(disk_part_t* part, fat32_privates_t* pr, dirent_t* dir, int level) {
    
    for(int i = 0; i < level+1; i++)
        puts("--");
    
    printf(" %u - %s\n", dir->type, dir->name);

    if(dir->type != DT_DIR)
        return;
    if(!strcmp(dir->name, ".") || !strcmp(dir->name, ".."))
        return;

    int n = 0;

    dirent_t* children = read_dir(part,pr,dir, &n);


    for(int i = 0; i < n; i++)
        log_tree(part, pr, &children[i], level+1);

    //free(children);
}



fs_t* fat32_detect(disk_part_t* part) {
    void* buf = malloc(block_size(part)+1);

    // boot record 
    read(part, 0, buf, 1);

    uint16_t total_clusters = *(uint16_t*)(buf + 0x13);
    uint16_t fat32_sig      = *(uint8_t *)(buf + 0x42);
    uint16_t bootable_sig   = *(uint16_t*)(buf + 510);

    // number of entries of the root directory 
    // 0 for fat32
    uint16_t root_entries = *(uint16_t*)(buf + 0x11);

    if(!total_clusters)
        total_clusters = *(uint32_t*)(buf + 0x20);


    // check if it is actually fat32 formatted
    if(bootable_sig != 0xAA55 
    ||(fat32_sig | 1) != 0x29
    || root_entries != 0
    ) {
        free(buf);
        return NULL;
    }


    uint16_t cluster_size = *(uint8_t *)(buf + 0x0d);
    uint16_t reserved     = *(uint16_t*)(buf + 0x0e);

    // lba of beginning of partition
    uint16_t lbabegin     = *(uint32_t*)(buf + 0x1c);

    // number of fats
    uint32_t tables_count = *(uint32_t*)(buf + 0x10);

///// extended boot record

    // number of sector per fat
    uint16_t fatsize      = *(uint32_t*)(buf + 0x24);

    // cluster id of root dir
    uint32_t root_dir_cluster = *(uint32_t*)(buf + 0x2c);

    fs_t* fs = malloc(sizeof(fs_t) + sizeof(fat32_privates_t));
    
    fs->part = part;
    fs->type = FS_TYPE_FAT;


    fat32_privates_t* pr = (void*)fs + sizeof(fs_t);

    pr->tables_count  = tables_count;
    pr->root_cluster  = root_dir_cluster;
    pr->clusters_size = cluster_size;
    pr->fat_begin     = reserved;
    pr->data_begin    = reserved + tables_count * fatsize;
    
    pr->fat_cache.n_entries = 0;


    fs->root = (dirent_t) {
        .cluster   = pr->root_cluster,
        .file_size = pr->clusters_size * block_size(part) * 8,
        .name      = "root",
        .type      = DT_DIR
    };

    log_tree(part,pr, &fs->root, 0);

    dirent_t* file = &fs->root.children[2];

    int size = 0;
    int i = 0;
    

    while((size = fat32_read_file_sector(fs, file, i, buf))) {
        ((uint8_t*)buf)[size] = 0;
        
        printf("%u: %s\n", i, buf);
        i++;
    }

    free(buf);
    return fs;
}



int fat32_read_file_sector(
    fs_t*     restrict fs, 
    dirent_t* restrict file,
    int offset, 
    char* buf) {
    assert(fs->type == FS_TYPE_FAT);


    fat32_privates_t* pr = (fat32_privates_t*)(fs+1);

    if(offset * block_size(fs->part) >= file->file_size)
        return 0;
    
    unsigned n_cluster = offset / pr->clusters_size;

    unsigned cluster_offset = offset % pr->clusters_size;


    uint32_t cluster = file->cluster;

    int last = 0;


    for(unsigned i = 0;i < n_cluster; i++) {
        assert(!last);
        cluster  = readFAT(fs->part, pr, cluster, &last);
    }


    read(fs->part, cluster_begin(cluster, pr) + cluster_offset, buf, 1);


    // file offset of the beginning of the file
    int byte_offset = offset * block_size(fs->part);


    if(byte_offset + block_size(fs->part) > file->file_size) {
        return file->file_size - byte_offset;
    }
    else
        return block_size(fs->part);
}
