#include "../../lib/assert.h"
#include "../../lib/logging.h"
#include "../../lib/dump.h"
#include "../../lib/math.h"
#include "../../lib/string.h"
#include "../../lib/sprintf.h"
#include "../../lib/utf16le.h"
#include "../../lib/panic.h"
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
    uint32_t clusters_size;
    uint32_t tables_count;
    uint32_t fat_begin;
    uint32_t data_begin;


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



static void handle_long_filename_entry(fat_dir_t* entry, char* name_buf) {

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

// return non-0 iif end of directory
static int parse_dir_entry(
        fs_t* fs, 
        fat_dir_t* dir, 
        int* long_entry, 
        dirent_t* entries, 
        int* cur_entry_id
) {

    // last entry
    if(dir->name[0] == 0)
        return 1;
    
    // unused entry
    if((uint8_t)dir->name[1] == 0xE5)
        return 0;

    dirent_t* cur_entry = &entries[*cur_entry_id];

    // long filename entry
    if(dir->attr == FAT32_LFN) {
        handle_long_filename_entry(dir, cur_entry->name);
        *long_entry = 1;
        return 0;
    }

    // short entry
    if(!*long_entry) {
        strncpy(cur_entry->name, dir->name, 8);
        char* ptr = cur_entry->name+7;
        
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
        cur_entry->name[255] = 0;
    // make sure it's null-terminated

    cur_entry->type       = fat_attr2fs_type(dir->attr);
    cur_entry->cluster    = dir->cluster_low | 
                ((uint32_t)dir->cluster_high << 16);
    
    cur_entry->file_size  = dir->file_size;
    cur_entry->children   = NULL;
    cur_entry->n_children = 0;
    cur_entry->fs         = fs;

// prepare for next entry
    (*cur_entry_id)++;

    *long_entry = 0;

    return 0;
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
dirent_t* fat32_read_dir(fs_t* fs, dirent_t* dir) {

    assert(fs->type == FS_TYPE_FAT);
    assert(dir->type == DT_DIR);

    disk_part_t* restrict part = fs->part;
    fat32_privates_t* restrict pr = (void*)(fs+1);

    
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

            if(parse_dir_entry(fs, dir, &long_entry, entries, &j)) {
                end = 1;
                break;
            } 

        }
        if(end)
            break;        

        cluster = readFAT(part, pr, cluster, &end);
    }

    // we took too much memory for sure
    entries = realloc(entries, sizeof(dirent_t) * j);
    dir->children = entries;
    dir->n_children = j;
    
    return entries;
}



fs_t* fat32_detect(disk_part_t* part, dirent_t* root) {
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
    fs->file_access_granularity = block_size(part);

    fs->open_file        = (void*)fat32_open_file;
    fs->close_file       = (void*)fat32_close_file;
    fs->read_file_sector = (void*)fat32_read_file_sector;
    fs->write_file_sector= (void*)fat32_write_file_sector;
    fs->seek             = (void*)fat32_seek;
    fs->read_dir         = (void*)fat32_read_dir;
    fs->file_cursor_size = sizeof(fat32_file_cursor_t);

    fs->root = root;

    root->children = NULL;
    root->cluster = root_dir_cluster;
    root->file_size = 0;
    root->n_children= 0;
    root->children = NULL;
    root->type = DT_DIR;
    root->fs = fs;


    fat32_privates_t* pr = (void*)fs + sizeof(fs_t);

    pr->tables_count  = tables_count;
    pr->clusters_size = cluster_size;
    pr->fat_begin     = reserved;
    pr->data_begin    = reserved + tables_count * fatsize;
    
    pr->fat_cache.n_entries = 0;

/*

    log_tree(fs, fs->root, 0);

    dirent_t* file = &fs->root->children[2];

    int size = 0;

    fat32_file_cursor_t* cur = fat32_open_file(file);


    while((size = fat32_read_file_sector(fs, cur, buf))) {
        ((uint8_t*)buf)[size] = 0;
        
        printf("%s", buf);
    }

    fat32_close_file(cur);
*/

    free(buf);
    return fs;
}



void fat32_open_file(dirent_t* restrict file, fat32_file_cursor_t* cur) {

    cur->cur_cluster        = file->cluster;
    cur->cur_cluster_offset = 0;
    cur->file               = file;
    cur->file_offset        = 0;
    
    cur->end = file->file_size == 0;
}


void fat32_close_file(fat32_file_cursor_t* cur) {
    (void) cur;
    // nothing to do,
    // we didn't allocate anything
}


/**
 * @brief advance by one sector the file 
 * cursor structure
 * 
 * @param part the partition
 * @param pr the fat32 privates
 * @param cur the curstor structure to advance
 */
static 
void advance_cursor(
        disk_part_t*         restrict part, 
        fat32_privates_t*    restrict pr, 
        fat32_file_cursor_t* restrict cur
    ) {

    cur->file_offset += block_size(part);
    
    // cursor reached the end
    if(cur->file_offset > cur->file->file_size) {
        cur->end = 1;
        return;
    }
    
    // advance cluster offset

    cur->cur_cluster_offset++;

    if(cur->cur_cluster_offset == pr->clusters_size) {
        // next sector!
        cur->cur_cluster_offset = 0;

        // unused
        int last;

        // advance cluster
        cur->cur_cluster = readFAT(part, pr, cur->cur_cluster, &last);
    }
}


int fat32_read_file_sector(
        fs_t*                restrict fs, 
        fat32_file_cursor_t* restrict cur, 
        void*                restrict buf
) {
    
    assert(fs->type == FS_TYPE_FAT);


    fat32_privates_t* pr = (fat32_privates_t*)(fs+1);


    if(cur->end)
        return 0;
    

    uint64_t lba = cluster_begin(cur->cur_cluster, pr) 
                               + cur->cur_cluster_offset;


    read(fs->part, lba, buf, 1);

    // calculate the number of read bytes

    int read_size;

    if(cur->file_offset + block_size(fs->part) 
                                > cur->file->file_size)
        read_size = cur->file->file_size - cur->file_offset;
    else
        read_size = block_size(fs->part);

    
    advance_cursor(fs->part, pr, cur);

    return read_size;
}


int fat32_write_file_sector(
        fs_t* restrict fs, 
        fat32_file_cursor_t* restrict cur, 
        const void* restrict buf,
        int size) {
    (void) fs;
    (void) cur;
    (void) buf;
    (void) size;
    panic("fat32_write_file_sector: unimplemented method");
    __builtin_unreachable();
}

int fat32_seek(
        fs_t* restrict fs, 
        fat32_file_cursor_t* restrict cur,
        uint64_t offset,
        int whence) {
    (void) fs;
    (void) cur;
    (void) offset;
    (void) whence;

    panic("fat32_seek: unimplemented method");
    __builtin_unreachable();
}

