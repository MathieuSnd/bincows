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

/**
 * this must be a
 * power of 2
 */
#define FAT_CACHE_SIZE 2048



typedef uint32_t cluster_t;

//////////////////////////
// functions prototypes //
//////////////////////////

void fat32_unmount(fs_t* fs);

int fat32_read_file_sectors(
        fs_t*   restrict fs, 
        file_t* restrict fd, 
        void*   restrict buf,
        uint64_t begin,
        size_t n
);

int fat32_write_file_sectors(
        fs_t* restrict fs, 
        file_t* restrict file, 
        const void* restrict buf,
        uint64_t begin,
        size_t n
);





// juste take the lower
// lba bits
static uint16_t lba_hash(uint64_t lba) {
    return lba & (FAT_CACHE_SIZE-1);
}

typedef struct block_cache_entry {
    uint64_t tag; // the whole lba
    char* buf;
    // buf dimension is exactly one sector
} block_cache_entry_t;


typedef struct block_cache {
    block_cache_entry_t entries[FAT_CACHE_SIZE];
} block_cache_t;

typedef struct {
    uint32_t cluster_size;
    uint32_t tables_count;
    cluster_t fat_begin;
    cluster_t data_begin;

    // size of the fat
    // in sectors
    uint32_t fat_size;

    /**
     * we use a circular allocation
     * algorithm. This field contains
     * the cursor of the FAT (the sector
     * index) and is incremented each time
     * the curent fat sector is full
     * 
     */
    uint32_t alloc_start;


    block_cache_t fat_cache;
} fat32_privates_t;




static 
void* read(disk_part_t* part, uint64_t lba, void* buf, size_t count) {
    lba += part->begin;


    assert(lba >= part->begin);
    assert(lba < part->end);

    
    part->interface->read(part->interface->driver, lba, buf, count);

    return buf;
}


static inline
void write(disk_part_t* part, uint64_t lba, const void* buf, size_t count) {
    lba += part->begin;
    //log_warn("lba=%lx", lba);
    assert(lba >= part->begin && lba <= part->end);

    part->interface->write(part->interface->driver, lba, buf, count);

}


static 
uint64_t __attribute__((const)) 
cluster_begin(cluster_t cluster, fat32_privates_t* restrict fp) {
    return ((cluster - 2) * fp->cluster_size) + fp->data_begin;
}



static
void setup_cache(block_cache_t* cache, unsigned sector_size) {
    void* buf = malloc(sector_size * FAT_CACHE_SIZE);

    for(unsigned i = 0; i < FAT_CACHE_SIZE; i++)
        cache->entries[i].buf = buf + sector_size * i;
}


static 
void cleanup_cache(block_cache_t* cache) {
    void* buf_begin = cache->entries[0].buf;
    
    free(buf_begin);
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
    
    // the cache is a hash set

    uint16_t h = lba_hash(lba);
    block_cache_entry_t* e = &cache->entries[h];


    if(e->tag == lba) {
        //log_debug("fat32: fat cache hit lba %lu", lba);
        return e->buf;
    }
    //log_debug("fat32: fat cache miss lba %lu", lba);

    return NULL;
}


/**
 * @brief reserve a cache entry associated 
 * with a given lba and return its buffer 
 * 
 * @param cache cache structure
 * @param lba entry lba
 * @return void* the buffer
 */
static
void* add_cache_entry(block_cache_t* cache, uint64_t lba) {
    

    block_cache_entry_t* e = &cache->entries[lba_hash(lba)];
    e->tag = lba;
    return e->buf;
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
void* fetch_fat_sector(
        fat32_privates_t* pr, 
        disk_part_t* part, 
        block_cache_t* cache, 
        uint64_t lba
) {
    static int i = 0;

    //log_debug("fetch fat sector %lu", lba);
    // assert that it is a fat sector
    assert(lba >= pr->fat_begin);
    assert(lba < pr->fat_begin + pr->fat_size);
    void* buf = cache_access(cache, lba);

    // cache hit
    if(buf) {
        return buf;
    }
    
    // cache miss

    buf = add_cache_entry(cache, lba);
    
    read(part, lba, buf, 1);


    return buf;
}


/**
 * @brief read a FAT entry
 * 
 * @param cluster the entry cluster
 * @return cluster_t a 28bit block offset for the 
 * entry or 0 if the cluster is the end of the chain
 */
static 
cluster_t readFAT(disk_part_t* part, 
                 fat32_privates_t* pr, 
                 uint32_t cluster
    ) {

    uint32_t offset = cluster * 4;
    uint64_t lba = pr->fat_begin + offset / block_size(part);
    uint32_t sector_offset = offset % block_size(part);
    
    void* sector = fetch_fat_sector(pr, part, &pr->fat_cache, lba);

    cluster_t ent = *(cluster_t *)(sector + sector_offset) & 0x0FFFFFFF;

    assert(ent != 0);
    assert(ent != 1);

    if(ent >= 0x0FFFFFF8) // last entry of the file
        return 0;

    return ent;
}


/**
 * @brief allocate a cluster in the FAT
 * table: find a cluster 
 * 
 * @param part 
 * @param pr 
 * @param cluster 
 * @return int 
 */
static
cluster_t allocFAT(disk_part_t* restrict part, 
                   fat32_privates_t* restrict pr
) {

    assert(pr->alloc_start >= pr->fat_begin);
    assert(pr->alloc_start < pr->fat_begin + pr->fat_size);

    uint32_t last = pr->fat_begin + pr->fat_size - 1;

    unsigned last_sector; 
    if(pr->alloc_start == pr->fat_begin) 
        last_sector = pr->fat_begin + pr->fat_size - 1;
    else
        last_sector = pr->alloc_start - 1;

    unsigned entries_per_sector = block_size(part) / 4;

    for(uint64_t lba  = pr->alloc_start; 
                 lba != last_sector;
                 lba = (lba == last) ? pr->fat_begin : lba + 1
                 
    ) {
        cluster_t* sector = fetch_fat_sector(pr, part, &pr->fat_cache, lba);

        unsigned begin = 0;

        if(lba == pr->fat_begin) 
            begin = 2; // 0th and 1st entries are reserved


        for(unsigned i = begin; i < entries_per_sector; i++) {
            if(sector[i] == 0) {
                // we found an unallocated cluster!
                pr->alloc_start = lba;
                //log_warn("allocfat %u", (lba - pr->fat_begin) * entries_per_sector + i);
                return (lba - pr->fat_begin) * entries_per_sector + i;
            }
        }
    }

    // the drive is full!
    return 0;
}



static 
void linkFAT(disk_part_t*     restrict part, 
            fat32_privates_t* restrict pr, 
            cluster_t from, cluster_t to
) {
    //log_warn("linkfat %u -> %u", from, to);
    uint32_t offset = from * 4;
    uint64_t lba = pr->fat_begin + offset / block_size(part);
    uint32_t sector_offset = offset % block_size(part);
    
    void* sector = fetch_fat_sector(pr, part, &pr->fat_cache, lba);

    cluster_t* ent = (cluster_t *)(sector + sector_offset);

//    assert((*ent & 0x0FFFFFFF) >= 0x0FFFFFF8); // last entry of the file
    assert((to & 0xf0000000) == 0);

    // don't modify reserved bits
    *ent = to | (*ent & 0xf0000000);


    //save_fat_
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



static void handle_long_filename_entry(
                const fat_dir_t* entry, 
                char* name_buf
) {
    const
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

// return value:
// 2: new entry
// 1: end of directory
// 0: no new entry,  not end of directory
static int parse_dir_entry(
        const fat_dir_t* dir, 
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

    if(!strcmp(cur_entry->name, ".") 
    || !strcmp(cur_entry->name, "..")) {
        // do not keep those.
        // they will be created virtually
        return 0;
    }

    cur_entry->type = fat_attr2fs_type(dir->attr);
    cur_entry->ino  = dir->cluster_low | 
                ((uint32_t)dir->cluster_high << 16);
    
    cur_entry->file_size  = dir->file_size;


// prepare for next entry
    (*cur_entry_id)++;

    *long_entry = 0;

    return 2;
}


dirent_t* fat32_read_dir(fs_t* fs, uint64_t cluster, size_t* n) {
    assert(fs->type == FS_TYPE_FAT);

    disk_part_t* restrict part = fs->part;
    fat32_privates_t* restrict pr = (void*)(fs+1);

    
    unsigned bufsize = block_size(part) * pr->cluster_size;
    uint8_t* buf = malloc(bufsize);

    int end = 0;


    dirent_t* entries = NULL;

    // current output entry id
    int j = 0;

    unsigned entries_per_cluster = bufsize / sizeof(fat_dir_t);

    // for long name entries
    int long_entry = 0;


    while(1) {

        entries = realloc(entries,
                    sizeof(dirent_t) * (j + entries_per_cluster));

        read(part, cluster_begin(cluster, pr), buf, pr->cluster_size);


        for(unsigned i = 0; i < entries_per_cluster; i++) {
            fat_dir_t* dir = (fat_dir_t*)buf + i;
    
            int v = parse_dir_entry(dir, &long_entry, entries, &j);

            if(v == 1) {
                end = 1;
                break;
            } 
        }

        if(end)
            break;

        cluster = readFAT(part, pr, cluster);
        if(!cluster)
            break;
    }

    free(buf);

    // we took too much memory for sure

    entries = realloc(entries, sizeof(dirent_t) * j);

    *n = j;

    return entries;
}


int fat32_update_dirent(fs_t* fs, uint64_t dir_cluster, const char* file_name, 
                    uint64_t file_cluster, uint64_t file_size) {
    
    assert(fs->type == FS_TYPE_FAT);

    disk_part_t* restrict part = fs->part;
    fat32_privates_t* restrict pr = (void*)(fs+1);

    
    unsigned bufsize = block_size(part) * pr->cluster_size;
    uint8_t* buf = malloc(bufsize);


    unsigned entries_per_cluster = bufsize / sizeof(fat_dir_t);

    // for long name entries
    int long_entry = 0;


    // if we were to include the filename in the modifiable
    // metadata, we would need to be able to modify multiple clusters:
    // an entry can cross multiple sectors 

    while(1) {
        uint64_t lba = cluster_begin(dir_cluster, pr);

        read(part, lba, buf, pr->cluster_size);


        dirent_t entry;
        for(unsigned i = 0; i < entries_per_cluster; i++) {
            fat_dir_t* dir = (fat_dir_t*)buf + i;
            int j = 0;
            int v = parse_dir_entry(dir, &long_entry, &entry, &j);
            assert(v != 1); // the entry should be present

            if(v == 2) {
                // new entry: we can check the file name
                if(!strcmp(file_name, entry.name)) {
                    // we found what we wanted
                    // let's modify the buffer then rewrite it
                    // to the drive


                    // for now, metadata only concern
                    // files
                    assert(entry.type == DT_REG);

                    dir->file_size = file_size;
                    dir->cluster_high = file_cluster >> 16;
                    dir->cluster_low  = file_cluster & 0xffff;
                    
                    write(part, lba, buf, pr->cluster_size);
                    

                    free(buf);
                    
                    return 0; // success
                }
            }
        }

        dir_cluster = readFAT(part, pr, dir_cluster);

        if(!dir_cluster)
            assert(0); 
        // the entry is not present
    }

    free(buf);
    
    // unsuccessful
    return 1;
}


void fat32_free_dirents(dirent_t* dir) {
    if(dir)
        free(dir);
}



fs_t* fat32_mount(disk_part_t* part) {
    void* buf = malloc(block_size(part)+1);

    // boot record 
    read(part, 0, buf, 1);

    uint32_t total_clusters = *(uint16_t*)(buf + 0x13);
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
    uint32_t fatsize      = *(uint32_t*)(buf + 0x24);

    // cluster id of root dir
    cluster_t root_dir_cluster = *(uint32_t*)(buf + 0x2c);

    fs_t* fs = malloc(sizeof(fs_t) + sizeof(fat32_privates_t));
    
    fs->part = part;
    fs->type = FS_TYPE_FAT;
    fs->file_access_granularity = block_size(part);
    fs->n_open_files = 0;

    fs->cacheable = 1;
    fs->read_only = 0;
    fs->seekable  = 1;

    fs->root_addr = root_dir_cluster;

    /**
     * cast to void* because of polymophism:
     * functions take file_t instead
     * of void*
     */
    fs->read_file_sectors  = fat32_read_file_sectors;
    fs->write_file_sectors = fat32_write_file_sectors;
    fs->read_dir           = fat32_read_dir;
    fs->update_dirent      = fat32_update_dirent;
    fs->free_dirents       = fat32_free_dirents;
    fs->unmount            = fat32_unmount;


    fat32_privates_t* pr = (void*)fs + sizeof(fs_t);

    pr->tables_count  = tables_count;
    pr->cluster_size  = cluster_size;
    pr->fat_size      = fatsize;
    pr->fat_begin     = reserved;
    pr->data_begin    = reserved + tables_count * fatsize;
    pr->alloc_start   = 540;
    //pr->fat_begin;
    
    // setup fat cache
    setup_cache(&pr->fat_cache, block_size(fs->part));


    free(buf);

    return fs;
}



void fat32_unmount(fs_t* fs) {
    fat32_privates_t* pr = (fat32_privates_t*)(fs+1);
    cleanup_cache(&pr->fat_cache);
    free(fs);

    // open files checking should be done by the vfs
}


/**
 * @brief fetch file cluster disk offset
 * with given file offset
 * 
 * 
 * @param fs fs structure
 * @param pr fs private (=fs+sizeof(fs_t))
 * @param fd file descriptor
 * @param off cluster offset in the file
 * @param end (output) if the returned 
 * cluster is the requested one, this field
 * is set to ~0llu. Else, it is set to the index
 * of the cluster chain's last element
 * @return cluster_t cluster disk offset
 * or the last cluster of the file chain if the 
 * cluster offset is beyond the cluster chain end.
 * In this case, output end is set to the file 
 * cluster offset of the returned cluster
 */
static 
cluster_t fetch_cluster(
        fs_t* restrict fs,
        fat32_privates_t* restrict pr,
        file_t* fd, 
        size_t off,
        uint64_t* end
) {

    cluster_t cluster = fd->addr;

    // find first cluster to read
    for(unsigned i = 0; i < off; i++) {
        cluster_t next = readFAT(fs->part, pr, cluster);
        if(next)
            cluster = next;
        else {
            *end = i;
            return cluster;
        }
    }

    *end = ~0llu;
    return cluster;
}



int fat32_read_file_sectors(
        fs_t*   restrict fs, 
        file_t* restrict fd, 
        void*   restrict buf,
        uint64_t begin,
        size_t n
) {
    
    assert(fs->type == FS_TYPE_FAT);


    fat32_privates_t* restrict pr = (fat32_privates_t*)(fs+1);

    assert((begin + n - 1) * pr->cluster_size < fd->file_size);

    // compute the first cluster and offset

    uint32_t cluster_offset = begin % pr->cluster_size;

    uint64_t clusterend = 0;
    uint32_t cluster = fetch_cluster(fs, pr, fd, begin / pr->cluster_size, &clusterend);

    // if this fails, the fs is f***ed up
    assert(clusterend == ~0llu);

    unsigned bsize = block_size(fs->part);
    

    while(n > 0) {
        uint64_t lba = cluster_begin(cluster, pr) 
                                + cluster_offset;

        unsigned read_size = 1;
        unsigned cluster_remaining = 
                    pr->cluster_size - cluster_offset;

        if(n >= cluster_remaining) {
            read_size = cluster_remaining;

            cluster_offset = 0;

            cluster = readFAT(fs->part, pr, cluster);
    
            n -= read_size;
            assert(cluster || !n);
        }
        // else, this is the last iteration

        read(fs->part, lba, buf, read_size);

        buf += bsize * read_size;
    }

    return n;
}


int fat32_write_file_sectors(
        fs_t* restrict fs, 
        file_t* restrict fd, 
        const void* restrict buf,
        uint64_t begin,
        size_t n
) {
    assert(fs->type == FS_TYPE_FAT);


    fat32_privates_t* restrict pr = (fat32_privates_t*)(fs+1);


    // compute the first cluster and offset

    uint32_t cluster_offset = begin % pr->cluster_size;

    uint64_t clusterend = 0;
    cluster_t cluster = fetch_cluster(fs, pr, fd, 
                            begin / pr->cluster_size, 
                            &clusterend);


    // the write operation is beyond file end.
    // allocate enough sectors and zero them
    if(clusterend != ~0llu) { // cluster = 0 <=> 
        // if this expression is false,
        // then the FS is broken
        assert((begin + n - 1) * pr->cluster_size * 
                        block_size(fs->part) >= fd->file_size);
        assert(clusterend < begin);
        for(unsigned i = clusterend; i <= begin; i++) {
            cluster_t cl = allocFAT(fs->part, pr);

            if(!cl) // the drive is full
                return 0;

            /**
             * @todo we might need to 
             * zero these freshly allocated 
             * sectors, along with the part of the
             * preceding sector that wasn't part of the file
             */
            
            // mark this sector as the end of the chain
            // to mark it alocated
            linkFAT(fs->part, pr, cl, 0xfffffff);

            // link cluster -> cl
            linkFAT(fs->part, pr, cluster, cl);
            // advance in the chain
            cluster = cl;
        }
    }

    unsigned bsize = block_size(fs->part);
    

    unsigned written = 0;

    while(n > 0) {
        uint64_t lba = cluster_begin(cluster, pr) 
                                + cluster_offset;

        unsigned write_size = 1;
        unsigned cluster_remaining = 
                    pr->cluster_size - cluster_offset;

        if(n >= cluster_remaining) {
            write_size = cluster_remaining;

            cluster_offset = 0;
        }
        // else, this is the last iteration


        write(fs->part, lba, buf, write_size);

        buf += bsize * write_size;

        n -= write_size;
        written++;


        if(n) {
            cluster_t next = readFAT(fs->part, pr, cluster);

            if(!next) {
                // we are at the file end
                // let's allocate another cluster
                next = allocFAT(fs->part, pr);

                if(!next) {
                    panic("ptn de merde");
                    // the drive is full
                    return n;
                }
                
                // mark the new cluster as the end of
                // the file chain
                linkFAT(fs->part, pr, next, 0xfffffff);

                // link the cluster in the file chain
                linkFAT(fs->part, pr, cluster, next);
            }
            cluster = next;
        }
    }

    return written;
}
