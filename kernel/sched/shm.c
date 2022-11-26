#include "sched.h"
#include "process.h"
#include "shm.h"

#include "../memory/heap.h"
#include "../memory/temp.h"
#include "../memory/paging.h"
#include "../memory/pmm.h"
#include "../memory/vmap.h"
#include "../lib/math.h"
#include "../int/idt.h"
#include "../sync/spinlock.h"


/**
 * global shm structure:
 * one of this entry represents a
 * shared memory object
 */
struct shm {
    shmid_t id;

    // number of SHM instances
    int n_insts;

    // physical address of the 1 GB
    // page directory (level 3 page structure)
    uint64_t pd_paddr;

    // byte size of the region
    uint32_t size;

    // if 0, don't free the physical memory when
    // removing the structure. The paging 
    // structures are freed anyway
    int pfree;
};




static struct shm* shms = NULL;
static unsigned n_shms = 0;


static fast_spinlock_t shm_lock;

static shmid_t shm_nextid(void) {

    static _Atomic shmid_t cur = 0;

    return ++cur;
}

// locks the structure and returns
// an element.
static struct shm* insert_shm(void) {
    assert(!interrupt_enable());
    spinlock_acquire(&shm_lock);

    shms = realloc(shms, (n_shms + 1) * sizeof(struct shm));

    return &shms[n_shms++];
}

static void remove_shm(struct shm* shm) {
    assert(!interrupt_enable());

    int i = shm - shms;

    if(i != (int)n_shms - 1) {
        shms[i] = shms[--n_shms];
    }
}

static struct shm* get_shm(shmid_t id) {
    // @todo make shms a sorted array
    // to access it in O(1)
    for(unsigned i = 0; i < n_shms; i++)
        if(shms[i].id == id)
            return &shms[i];
    return NULL;
}


static void* map_shm(struct shm* shm, pid_t pid) {
    assert(!interrupt_enable());

    // acquire the process
    process_t* proc = sched_get_process(pid);

    uint64_t target_pd = shm->pd_paddr;

    if(!proc) {
        return NULL;
    }

    uint64_t master_pd = proc->mem_map.shared_pd;

    // search available slot
    uint64_t* tr_master_pd = translate_address((void*)master_pd);

    void* vaddr = NULL;

    // @todo this seems dirty,
    // might move that to /memory/paging.h
    for(int i = 0; i < 512; i++) {
        int av = !(tr_master_pd[i] & PRESENT_ENTRY);
        
        if(av) {
            vaddr = (void*)(USER_SHARED_BEGIN | (i * PAGE_R1_SIZE));
        
            // map the shm
            uint64_t flags = PL_XD | PL_US | PL_RW | PRESENT_ENTRY;

            tr_master_pd[i] = flags | target_pd;
            break;
        }
    }

    spinlock_release(&proc->lock);

    
    return vaddr;
}


// 0 on success
static int unmap_shm(process_t* proc, void* vaddr) {
    assert(!interrupt_enable());

    assert((uint64_t)vaddr >= USER_SHARED_BEGIN);
    assert((uint64_t)vaddr <= USER_SHARED_END - PAGE_R1_SIZE);

    assert(proc);

    uint64_t master_pd = proc->mem_map.shared_pd;

    uint64_t* tr_master_pd = translate_address((void*)master_pd);


    int i = ((uint64_t)vaddr - USER_SHARED_BEGIN) / PAGE_R1_SIZE;

    assert(i >= 0);
    assert(i < 512);

    int present = tr_master_pd[i] & PRESENT_ENTRY;

    // the shm shold be present
    assert(present);

    tr_master_pd[i] = 0;



    spinlock_release(&proc->lock);

    
    return 0;
}




struct shm_instance* 
shm_create_from_kernel(size_t initial_size, void* base, int pfree) {
    assert(!interrupt_enable());

    if(initial_size == 0 || initial_size > SHM_SIZE_MAX)
        return NULL;
        
    // get the level 2 page directory
    assert_aligned(base, PAGE_R1_SIZE);
    uint64_t pd = get_pd(base, 1);

    if(!pd)
        return NULL;

    // spinlock acquired    
    struct shm* shm = insert_shm();

    shmid_t id = shm_nextid();

    *shm = (struct shm) {
        .id       = id,
        .n_insts  = 1,
        .pd_paddr = pd,
        .size     = initial_size,
        .pfree    = pfree,
    };

    spinlock_release(&shm_lock);

    struct shm_instance* ins = malloc(sizeof(struct shm_instance));

    *ins = (struct shm_instance) {
        .target = id,
        .pid    = 0,
        .vaddr  = NULL,
    };

    return ins;
}


void shm_cleanup(void) {
    if(shms)
        free(shms);
}



static void free_shm(struct shm* shm) {
    // map to higher half and deep
    // free the memory range using
    // the page manager
    assert(!interrupt_enable());

    free_r1_region(shm->pd_paddr, shm->pfree);
}

// return the page directory of the freshly
// allocated memory range
static uint64_t shm_alloc(size_t size) {
    assert(!interrupt_enable());
    
    // allocate the virtual memory range in temp space
    void* temp = temp_lock();
    assert(temp);

    size_t pages = CEIL_DIV(size, 0x1000);


    // allocate physical memory inside the range
    alloc_pages(
        temp,
        pages,
        PRESENT_ENTRY | PL_XD | PL_RW | PL_US
    );

    uint64_t master_pd = get_master_pd(temp);

    unmap_master_region(temp);
    physfree(master_pd);


    // unmap_pages((uint64_t)temp, pages, 0);

    uint64_t pd = get_pd(temp, 1);

    temp_release();

    return pd;
}



struct shm_instance* shm_create(size_t initial_size, pid_t pid) {

    if(initial_size == 0 || initial_size > SHM_SIZE_MAX)
        return NULL;

    void* vaddr = NULL;

    uint64_t rf = get_rflags();
    _cli();

    uint64_t pd = shm_alloc(initial_size);

    if(!pd) {
        set_rflags(rf);
        return NULL;
    }


    shmid_t id = shm_nextid();

    {
        // spinlock acquired
        struct shm* shm = insert_shm();

        *shm = (struct shm) {
            .id       = id,
            .n_insts  = 1,
            .pd_paddr = pd,
            .size     = initial_size,
            .pfree    = 1,
        };

        void* vaddr = map_shm(shm, pid);

        if(!vaddr) {
            remove_shm(shm);
            spinlock_release(&shm_lock);
            set_rflags(rf);
            return NULL;
        }

        spinlock_release(&shm_lock);
    }

    set_rflags(rf);

    struct shm_instance* ins = malloc(sizeof(struct shm_instance*));

    *ins = (struct shm_instance) {
        .target = id,
        .vaddr  = vaddr,
        .pid    = pid,
    };

    return ins;
}


struct shm_instance* shm_open(shmid_t id, pid_t pid) {
    int found;
    void* vaddr = NULL;

    uint64_t rf = get_rflags();
    _cli();
    spinlock_acquire(&shm_lock);

    struct shm* shm = get_shm(id);

    if(!shm) {
        found = 0;
    }
    else {
        found = 1;

        // map to memory
        vaddr = map_shm(shm, pid);

        if(vaddr) {
            shm->n_insts++;
        }
    }

    spinlock_release(&shm_lock);
    set_rflags(rf);

    if(!found)
        return NULL;
    if(vaddr == NULL)
        return NULL;

    
    struct shm_instance* ins = malloc(sizeof(struct shm_instance));
    *ins = (struct shm_instance) {
        .target = id,
        .vaddr  = vaddr,
        .pid    = pid,
    };
    
    return ins;
}


int shm_close(process_t* proc, struct shm_instance* ins) {
    assert(ins);
    shmid_t id = ins->target;
    int found;


    uint64_t rf = get_rflags();
    _cli();
    spinlock_acquire(&shm_lock);

    struct shm* shm = get_shm(id);

    found = shm != NULL;
    // @todo remove this debug assert
    assert(shm);

    assert(shm->n_insts > 0);

    shm->n_insts--;

    if(!shm->n_insts) {
        // remove it
        free_shm(shm);
        remove_shm(shm);
    }

    int unmap_error = 0;


    if(proc)
        unmap_error = unmap_shm(proc, ins->vaddr);

    free(ins);

    spinlock_release(&shm_lock);
    set_rflags(rf);

    return (found && !unmap_error) ? 0 : -1;
}

