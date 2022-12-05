#include "../../lib/assert.h"
#include "../../lib/logging.h"
#include "../../lib/dump.h"
#include "../../lib/math.h"
#include "../../lib/string.h"
#include "../../lib/sprintf.h"
#include "../../lib/utf16le.h"
#include "../../lib/panic.h"

#include "../../memory/heap.h"
#include "../../memory/vmap.h"

#include "fat32.h"
#include "specs.h"

/**
 * this must be a
 * power of 2
 */
#define FAT_CACHE_SIZE 2048

int fat32_truncate_file(fs_t* fs, file_t* restrict  file, uint64_t file_size);


// stolen from Linux source
static inline 
unsigned char fat_checksum(const uint8_t* name)
{
	unsigned char s = name[0];
	s = (s<<7) + (s>>1) + name[1];	s = (s<<7) + (s>>1) + name[2];
	s = (s<<7) + (s>>1) + name[3];	s = (s<<7) + (s>>1) + name[4];
	s = (s<<7) + (s>>1) + name[5];	s = (s<<7) + (s>>1) + name[6];
	s = (s<<7) + (s>>1) + name[7];	s = (s<<7) + (s>>1) + name[8];
	s = (s<<7) + (s>>1) + name[9];	s = (s<<7) + (s>>1) + name[10];
	return s;
}


typedef uint32_t cluster_t;

//////////////////////////
// functions prototypes //
//////////////////////////

void fat32_unmount(fs_t* fs);

int fat32_read_file_sectors(
        fs_t*   restrict fs, 
        const file_t* restrict fd, 
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
__attribute__((const))
static uint16_t lba_hash(uint64_t lba) {
    return lba & (FAT_CACHE_SIZE-1);
}

typedef struct block_cache_entry {
    uint64_t tag: 63; // the whole lba
    int modified: 1; // write back policy
    // buf dimension is exactly one sector
} block_cache_entry_t;


typedef struct block_cache {
    block_cache_entry_t entries[FAT_CACHE_SIZE];
    void* buf;
    size_t sector_size_shift;

    //unsigned 
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
void* async_read(
            const disk_part_t* restrict part, 
            uint64_t lba, 
            void* restrict buf, 
            size_t count
) {
    lba += part->begin;


    assert(lba >= part->begin);
    assert(lba < part->end);

    assert(buf);

    
    part->interface->async_read(part->interface->driver, lba, buf, count);

    return buf;
}


static 
void* read(
            const disk_part_t* restrict part, 
            uint64_t lba, 
            void* restrict buf, 
            size_t count
) {
    async_read(part, lba, buf, count);
    
    part->interface->sync(part->interface->driver);
    
    return buf;
}



static inline
void write(
        const disk_part_t* restrict part, 
        uint64_t lba, 
        const void* restrict buf, 
        size_t count
) {
    lba += part->begin;
    assert(lba >= part->begin && lba <= part->end);

    part->interface->write(part->interface->driver, lba, buf, count);

}


static 
uint64_t __attribute__((const)) 
cluster_begin(cluster_t cluster, fat32_privates_t* restrict fp) {
    return ((cluster - 2) * fp->cluster_size) + fp->data_begin;
}



static
void setup_cache(block_cache_t* cache, unsigned sector_size_shift) {
    cache->buf = malloc(FAT_CACHE_SIZE << sector_size_shift);
    cache->sector_size_shift = sector_size_shift;

    for(unsigned i = 0; i < FAT_CACHE_SIZE; i++) {
        cache->entries[i].modified = 0;
        cache->entries[i].tag = 0;
    }
}


static 
void cleanup_cache(block_cache_t* cache) {
    void* buf_begin = cache->buf;    
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
        return cache->buf + (lba << cache->sector_size_shift);
    }
    //log_debug("fat32: fat cache miss lba %lu", lba);

    return NULL;
}

// flush cache
static
void flush_fat_cache(disk_part_t* part, block_cache_t* cache) {
    int s = 0;

    
    for(unsigned i = 0; i < FAT_CACHE_SIZE; i++) {
        if(cache->entries[i].modified) {
            s++;
            cache->entries[i].modified = 0;
            write(
                part, 
                cache->entries[i].tag, 
                cache->buf + (i << cache->sector_size_shift), 
                1
            );
        }
        //cache->entries[i].tag = 0;
    }
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
void* add_cache_entry(
        const disk_part_t* restrict part, 
        block_cache_t*     restrict cache, 
        uint64_t lba
) {
    block_cache_entry_t* e = &cache->entries[lba_hash(lba)];

    void* buf = cache->buf + (lba_hash(lba) << cache->sector_size_shift);

    // before replacing the entry,
    // we write it back to memory if needed
    if(e->modified) {
        write(part, e->tag, buf, 1);
        e->modified = 0;

    }

    e->tag = lba;
    return buf;
}


static 
void mark_modified(block_cache_t* cache, uint64_t lba) {
    block_cache_entry_t* e = &cache->entries[lba_hash(lba)];
    assert(e->tag == lba);

    e->modified = 1;
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
        const fat32_privates_t* restrict pr, 
        const disk_part_t*      restrict part, 
        block_cache_t*          restrict cache, 
        uint64_t lba
) {

    // assert that it is a fat sector
    assert(lba >= pr->fat_begin);
    assert(lba < pr->fat_begin + pr->fat_size);
    void* buf = cache_access(cache, lba);

    // cache hit
    if(buf) {
        return buf;
    }
    
    // cache miss

    buf = add_cache_entry(part, cache, lba);
    
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
cluster_t readFAT(
        const disk_part_t* restrict part, 
        fat32_privates_t*  restrict pr, 
        cluster_t cluster
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
cluster_t allocFAT(const disk_part_t* restrict part, 
                   fat32_privates_t*  restrict pr
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
                return (lba - pr->fat_begin) * entries_per_sector + i;
            }
        }
    }

    // the drive is full!
    return 0;
}


static 
void linkFAT(
        const disk_part_t* restrict part, 
        fat32_privates_t*  restrict pr, 
        cluster_t from, cluster_t to
) {
    uint32_t offset = from * 4;
    uint64_t lba = pr->fat_begin + offset / block_size(part);
    uint32_t sector_offset = offset % block_size(part);
    
    void* sector = fetch_fat_sector(pr, part, &pr->fat_cache, lba);

    cluster_t* ent = (cluster_t *)(sector + sector_offset);

    assert((*ent & 0x0FFFFFFF) >= 0x0FFFFFF8 
                || *ent == 0); // last entry of the file
    assert((to & 0xf0000000) == 0);

    // don't modify reserved bits
    *ent = to | (*ent & 0xf0000000);

    // save
    //write(part, lba, sector, 1);
    mark_modified(&pr->fat_cache, lba);
}


static 
uint32_t __attribute__((const)) fat_attr2fs_type(uint32_t attr) {
    uint32_t ret = 0;

    if(attr & FAT32_DIRECTORY)
        ret |= DT_DIR;
    else
        ret |= DT_REG; // file

    return ret;
}

static 
uint32_t __attribute__((const)) fs_type2fat_attr(int type)  {
    uint32_t ret = 0;

    if(type == DT_DIR)
        ret |= FAT32_DIRECTORY;
    else if(type == DT_REG)
        ;
    else
        panic("unreachable");

    return ret;
}




/**
 * @brief same as readFAT if the the cluster
 * is not the end of the chain
 * 
 * Otherwise, return a new cluster, or 0 if 
 * the drive is full
 * 
 * @param part 
 * @param pr 
 * @param cluster 
 * @return cluster_t 
 */
static 
cluster_t read_or_allocateFAT(
        const disk_part_t*      restrict part, 
        fat32_privates_t* restrict pr, 
        uint32_t cluster
) {

    cluster_t next = readFAT(part, pr, cluster);

    if(!next) {
        // we are at the file end
        // let's allocate another cluster
        next = allocFAT(part, pr);

        if(!next) {
            // the drive is full
            return 0;
        }
        
        // mark the new cluster as the end of
        // the file chain
        linkFAT(part, pr, next, 0xfffffff);

        // link the cluster in the file chain
        linkFAT(part, pr, cluster, next);

    }
    return next;
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
        const fat_dir_t* restrict dir, 
        int*             restrict long_entry, 
        dirent_t*        restrict entries, 
        int*             restrict cur_entry_id
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

    // no read only files for now
    cur_entry->rights = (file_rights_t) {
        .exec = 1,
        .read = 1,
        .write = 1,
        .truncatable = 1,
        .seekable = 1,
    };

    


// prepare for next entry
    (*cur_entry_id)++;

    *long_entry = 0;

    return 2;
}


dirent_t* fat32_read_dir(
            fs_t* restrict fs, 
            uint64_t cluster, 
            size_t* restrict n
) {
    assert(fs->type == FS_TYPE_FAT);

    disk_part_t* restrict part = fs->part;
    fat32_privates_t* restrict pr = (void*)(fs+1);

    
    unsigned bufsize = block_size(part) * pr->cluster_size;
    uint8_t* buf = malloc(bufsize + 8);

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


/**
 * @brief emplace a dirent in a directory
 * 
 * @param buf the current cluster buffer
 * @param ent the entry to emplace
 * @param lba the lba of the current 
 * @param cur current entry offset
 * @param entries_per_cluster
 * @param cur_cluster current cluster
 * @param part disk partition
 * @param pr fat32 privates
 * @return int the next value of entry_offset
 * -1 if the drive is full and therefore no
 * entry can be inserted anymore
 */
static 
int emplace_dirent(
        void*      restrict buf, 
        fat_dir_t* restrict ent,
        int        entry_offset, 
        int        entries_per_cluster,
        cluster_t*  restrict cur_cluster,
        const disk_part_t*      restrict part,
        fat32_privates_t* restrict pr
) {
    ((fat_dir_t *)buf)[entry_offset++] = *ent;

    if(entry_offset == entries_per_cluster) {
        entry_offset = 0;
        // write cluster back
        write(
            part, 
            cluster_begin(*cur_cluster, pr), 
            buf, 
            pr->cluster_size
        );


        *cur_cluster = read_or_allocateFAT(part, pr, *cur_cluster);
        if(! *cur_cluster)
            return -1; // drive full
        

        // zero the next cluster
        memset(buf, 0, entries_per_cluster * sizeof(fat_dir_t));
    }
    
    return entry_offset;
}


int fat32_add_dirent(
            fs_t* restrict fs, 
            uint64_t dir_cluster, 
            const char* file_name, 
            uint64_t* file_cluster, 
            unsigned type
) {


    // we go through the dir sequentially
    // and we only keep track of the current 
    // dir cluster
    cluster_t current_dir_cluster = dir_cluster;
    
    assert(fs->type == FS_TYPE_FAT);

    disk_part_t* restrict part = fs->part;
    fat32_privates_t* restrict pr = (void*)(fs+1);


    // empty file: no cluster allocated until accessed
    *file_cluster = 0;



    unsigned bufsize = block_size(part) * pr->cluster_size;
    uint8_t* buf = malloc(bufsize);


    unsigned entries_per_cluster = bufsize / sizeof(fat_dir_t);

    // for long name entries
    int long_entry = 0;

    // interator in the
    // first inner loop. in the end,
    // it contains the entry offset
    unsigned entry_cluster_offset;

    // find the end of the dir

    if(dir_cluster == 1174) {
        sleep(10);
    }

    while(1) {
        uint64_t lba = cluster_begin(current_dir_cluster, pr);

        read(part, lba, buf, pr->cluster_size);

        int v = 0;

        dirent_t entry;

        
        for(entry_cluster_offset = 0; 
            entry_cluster_offset < entries_per_cluster; 
            entry_cluster_offset++
        ) {
            fat_dir_t* dir = (fat_dir_t*)buf + entry_cluster_offset;
            int j = 0;
            v = parse_dir_entry(dir, &long_entry, &entry, &j);

            if(v == 1)
                break;
        }

        if(v == 1) // we found the end
            break;

        current_dir_cluster = readFAT(part, pr, current_dir_cluster);

        if(!current_dir_cluster)
            assert(0);
    }

    // we found the end!

    // first compute the short filename entry
    fat_dir_t sfn = {
        .attr = fs_type2fat_attr(type),
                          // no cluster allocated yet
        .cluster_high = 0,//new_cluster >> 16,
        .cluster_low  = 0,//new_cluster & 0xffff,
        .date0 = 0,
        .date1 = 0,
        .date2 = 0,
        .date3 = 0,
        .date4 = 0,
        .date5 = 0,
        .reserved = 0,
        .file_size = 0, // empty file
        .name  ={'I','S','S','O','U',' ',' ',' ',' ',' ',' '},
    };


    // fat checksum of the 'ISSOU      ' string
    int sfn_checksum = 0x81; 

 
    // we can easily suppose that
    // we will always need long file name entries.
    
    
    unsigned n_entries = CEIL_DIV(strlen(file_name), 13);


    // create the name buffer
    uint16_t* namebuf = malloc(n_entries * 13 * sizeof(uint16_t));

    // padding 
    memset(namebuf, 0xff, n_entries * 13 * sizeof(uint16_t));
    // chars
    ascii2utf16le(namebuf, file_name, n_entries * 13 * sizeof(uint16_t));

    uint16_t* name_ptr =  namebuf;
    
    // I guess entries have to be inserted backward
    for(int i = n_entries-1; i >= 0; i--) {

        uint8_t order = i+1;

        if(order == n_entries)
            order |= 0x40;

        fat_long_filename_t lfn_entry = {
            .order    = order,
            .attr     = FAT32_LFN,
            .zero     = 0,
            .type     = 0,
            .checksum = sfn_checksum,
            .chars0 = {
                name_ptr[0], name_ptr[1], name_ptr[2], 
                name_ptr[3], name_ptr[4],
            },
            .chars1 = {
                name_ptr[5], name_ptr[6], name_ptr[7], 
                name_ptr[8], name_ptr[9], name_ptr[10],
            },
            .chars2 = {name_ptr[11], name_ptr[12]},
        };

        name_ptr += 13;


        // now emplace the entry
        entry_cluster_offset = emplace_dirent(
                buf, 
                (fat_dir_t*)&lfn_entry, 
                entry_cluster_offset,
                entries_per_cluster,
                &current_dir_cluster,
                part,
                pr
        );

        assert(entry_cluster_offset != (unsigned int)-1);
    }

    // finaly emplace the sfn entry

    entry_cluster_offset = emplace_dirent(
            buf, 
            &sfn, 
            entry_cluster_offset,
            entries_per_cluster,
            &current_dir_cluster,
            part,
            pr
    );
    assert(entry_cluster_offset != (unsigned int)-1);

    // unless emplace_dirent just filled
    // the current cluster, in which case
    // it would return entry_cluster_offset = 0,
    // we need to write the current cluster back
    if(entry_cluster_offset) {

        write(
            part, 
            cluster_begin(current_dir_cluster, pr), 
            buf, 
            pr->cluster_size
        );
    }

    

    free(namebuf);


    free(buf);
    return 0; // successs
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

#include "../../lib/registers.h"

fs_t* fat32_mount(disk_part_t* part) {


    //assert(0);
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
    assert(interrupt_enable() && "interrupts are not enabled");
    
    fs->part = part;
    fs->type = FS_TYPE_FAT;
    fs->file_access_granularity = block_size(part);
    fs->n_open_files = 0;

    fs->cacheable   = 1;

    fs->root_addr = root_dir_cluster;

    /**
     * cast to void* because of polymophism:
     * functions take file_t instead
     * of void*
     */
    fs->open_file          = NULL;
    fs->close_file         = NULL;
    fs->open_instance      = NULL;
    fs->close_instance     = NULL;
    fs->read_file_sectors  = fat32_read_file_sectors;
    fs->write_file_sectors = fat32_write_file_sectors;
    fs->read_dir           = fat32_read_dir;
    fs->update_dirent      = fat32_update_dirent;
    fs->free_dirents       = fat32_free_dirents;
    fs->add_dirent         = fat32_add_dirent;
    fs->unmount            = fat32_unmount;
    fs->truncate_file      = fat32_truncate_file;


    fat32_privates_t* pr = (void*)fs + sizeof(fs_t);

    pr->tables_count  = tables_count;
    pr->cluster_size  = cluster_size;
    pr->fat_size      = fatsize;
    pr->fat_begin     = reserved;
    pr->data_begin    = reserved + tables_count * fatsize;
    pr->alloc_start   = pr->fat_begin;
    //pr->fat_begin;
    
    // setup fat cache
    setup_cache(&pr->fat_cache, fs->part->interface->lbashift);


    free(buf);

    fs->name = strdup("fat32");

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
        const file_t* fd, 
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


static void alloc_file_cluster(
                disk_part_t*      restrict part, 
                fat32_privates_t* restrict pr,
                file_t*           restrict fd
) {
    assert(fd->file_size == 0);
    fd->addr = allocFAT(part, pr);
    linkFAT(part, pr, fd->addr, 0xfffffff);
}




static 
int extend_file(fs_t* fs, 
                file_t* restrict  file, 
                uint64_t file_size
) {
    assert(file_size > file->file_size);

    unsigned extend = file_size - file->file_size;

    fat32_privates_t* pr = (void*)(fs+1);

    if(!file->addr) {
        // empty file: let's allocate
        // its first sector
        alloc_file_cluster(fs->part, pr, file);
    }

    if(extend > 1 >> 31)
        return 1; // too big
    const unsigned max_buf_size = 8 * 1024 * 1024;

    int sz = extend > max_buf_size ? max_buf_size : extend;
    void* buf = malloc(sz);

    memset(buf, 0, sz);

    while(extend) {
        unsigned to_write = extend > max_buf_size ? max_buf_size : extend;
        extend -= to_write;
            
        // we need to extend the file
        fat32_write_file_sectors(
            fs,
            file,
            buf,
            file->file_size,
            extend
        );
    }

    return 0; // success
}



static 
int trunc_file(fs_t* fs, 
                const file_t* restrict  file, 
                uint64_t file_size
) {
    fat32_privates_t* restrict pr = (void*)(fs+1);
    disk_part_t* restrict part = fs->part;

    assert(file_size < file->file_size);


    const unsigned cluster_bsize = pr->cluster_size * block_size(fs->part);

    int trunc_clusters = 
        file->file_size / cluster_bsize  // old cluster count
      - file_size       / cluster_bsize; // new cluster count

    assert(trunc_clusters >= 0);

    if(file->file_size - file_size > cluster_bsize)
        return -1; // too big

    // if the new limit is in the middle of the cluster
    // of the old limit, there is no trucation to do
    else if(trunc_clusters == 0)
        return 0; // success

    

    // fetch the new last cluster of the file
    uint64_t clusterend = 0;
    uint32_t cluster = fetch_cluster(
                        fs, pr, file, 
                        file_size / pr->cluster_size, 
                        &clusterend);

    // if this fails, the fs is f***ed up
    assert(clusterend != ~0llu);

    // if this expression is false,
    // then the FS is broken
    // cluster endd is the index of the requested cluster
    assert(clusterend == file_size / pr->cluster_size);


    // mark the last cluster as the chain end
    linkFAT(part, pr, cluster, 0x0fffffff);




    // free the rest of the cluster chain
    for(int i = 0; i < trunc_clusters; i++) {
        // get the next one
        cluster_t next = readFAT(part, pr, cluster);

        // mark this cluster as free
        linkFAT(part, pr, cluster, 0);

        if(i != trunc_clusters - 1) {
            // if this is not the last cluster,
            // the next one should be allocated
            assert(next != 0);
        }
        else {
            // if this is the last cluster,
            // it should be the last one
            assert(next == 0);
        }

        cluster = next;
    }

    return 0; // success
}


int fat32_truncate_file(fs_t* fs, 
            file_t* restrict  file, uint64_t file_size
) {
    assert(fs->type == FS_TYPE_FAT);


    if(file_size > file->file_size)  {
        return extend_file(fs, file, file_size);
    }
    
    else if(file_size < file->file_size) 
        return trunc_file(fs, file, file_size);

    // success
    return 0;
}



int fat32_read_file_sectors(
        fs_t*   restrict fs, 
        const file_t* restrict fd, 
        void*   restrict buf,
        uint64_t begin,
        size_t n
) {
    
    assert(fs->type == FS_TYPE_FAT);

    assert(fd->addr);


    fat32_privates_t* restrict pr = (fat32_privates_t*)(fs+1);

    assert((begin + n - 1) * pr->cluster_size < fd->file_size);

    // compute the first cluster and offset

    uint32_t cluster_offset = begin % pr->cluster_size;

    uint64_t clusterend = 0;
    uint32_t cluster = fetch_cluster(fs, pr, fd, begin / pr->cluster_size, &clusterend);

    // if this fails, the fs is f***ed up
    assert(clusterend == ~0llu);

    unsigned bsize = block_size(fs->part);

    int rd = n;
    

    while(n > 0) {
        uint64_t lba = cluster_begin(cluster, pr) 
                                + cluster_offset;

        unsigned read_size = 0;

        // read multiple clusters
        // if contiguous
        while(n) {
            unsigned cluster_remaining = 
                        pr->cluster_size - cluster_offset;


            if(n < cluster_remaining) {
                read_size += n;
                n = 0;
                // this is the last iteration
                break;
            }
            else {
                read_size += cluster_remaining;

                cluster_offset = 0;
                
                unsigned old = cluster;

                cluster = readFAT(fs->part, pr, cluster);
                
                n -= cluster_remaining;

                assert(cluster || !n);

                if(cluster != old + 1) {
                    // not contiguous blocks
                    break;
                }
        
            }
        }
        async_read(fs->part, lba, buf, read_size);

        buf += bsize * read_size;
    }

    fs->part->interface->sync(fs->part->interface->driver);

    return rd;
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
    uint32_t first_cluster  = begin / pr->cluster_size; 
    uint32_t cluster_offset = begin % pr->cluster_size;

    uint64_t clusterend = 0;

    if(!fd->addr) {
        // the file is empty, no assicated cluster
        // allocate the file's first cluster
        alloc_file_cluster(fs->part, pr, fd);
    }
    
    cluster_t cluster = fetch_cluster(fs, pr, fd, first_cluster, &clusterend);

    assert(cluster);

    // the write operation is beyond file end.
    // allocate enough sectors and zero them
    if(clusterend != ~0llu) { // cluster = 0 <=> 
        // if this expression is false,
        // then the FS is broken

        assert((begin + n - 1) * pr->cluster_size * 
                        block_size(fs->part) >= fd->file_size);
        assert(clusterend < first_cluster);

        for(unsigned i = clusterend; i < first_cluster; i++) {
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
        // cluster 0 is reserved.
        assert(cluster);
        uint64_t lba = cluster_begin(cluster, pr) 
                                + cluster_offset;

        unsigned write_size = 1;
        unsigned cluster_remaining = 
                    pr->cluster_size - cluster_offset;
        if(n > cluster_remaining) {
            write_size = cluster_remaining;

            cluster_offset = 0;
        }
        else
            // this is the last iteration
            write_size = n;

        write(fs->part, lba, buf, write_size);

        buf += bsize * write_size;

        n -= write_size;
        written++;


        if(n) {
            cluster = read_or_allocateFAT(fs->part, pr, cluster);

            if(!cluster) {
                // the drive is full
                return n;
            }
        }
    }


    flush_fat_cache(fs->part, &pr->fat_cache);

    return written;
}
