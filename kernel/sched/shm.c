#include "sched.h"
#include "shm.h"
#include "../memory/heap.h"
#include "../memory/temp.h"
#include "../memory/paging.h"
#include "../lib/math.h"
#include "../int/idt.h"
#include "../sync/spinlock.h"


static struct shm* shms = NULL;
static unsigned n_shms = 0;


static fast_spinlock_t shm_lock;

static shmid_t shm_nextid(void) {
    assert(interrupt_enable());
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

    // @todo check if it is right
    int i = (shm - shms) / sizeof(struct shm);

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

struct shm_instance* shm_create_from(size_t initial_size, void* base) {
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
        .size     = initial_size
    };

    spinlock_release(&shm_lock);

    struct shm_instance* ins = malloc(sizeof(struct shm_instance));

    *ins = (struct shm_instance) {
        .target = id,
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

    free_r1_region(shm->pd_paddr);

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

    unmap_pages((uint64_t)temp, pages, 0);

    uint64_t pd = get_pd(temp, 1);

    temp_release();

    return pd;
}



struct shm_instance* shm_create(size_t initial_size) {

    if(initial_size == 0 || initial_size > SHM_SIZE_MAX)
        return NULL;

    uint64_t rf = get_rflags();
    _cli();

    uint64_t pd = shm_alloc(initial_size);
    if(!pd) {
        _sti();
        return NULL;
    }

    // spinlock acquired
    struct shm* shm = insert_shm();

    *shm = (struct shm) {
        .id       = shm_nextid(),
        .n_insts  = 1,
        .pd_paddr = pd,
        .size     = initial_size
    };
    set_rflags(rf);

    struct shm_instance* inst = malloc(sizeof(struct shm_instance*));

    *inst = (struct shm_instance) {
        .target = shm->id
    };

    return inst;
}


struct shm_instance* shm_open(shmid_t id) {
    int found;

    uint64_t rf = get_rflags();
    _cli();
    spinlock_acquire(&shm_lock);

    struct shm* shm = get_shm(id);

    if(!shm) {
        found = 0;
    }
    else {
        found = 1;

        shm->n_insts++;
    }

    spinlock_release(&shm_lock);
    set_rflags(rf);

    if(!found)
        return NULL;
    
    struct shm_instance* ins = malloc(sizeof(struct shm_instance));
    ins->target = id;
    
    return ins;
}


int shm_close(const struct shm_instance* ins) {
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

    spinlock_release(&shm_lock);
    set_rflags(rf);

    return found ? 0 : -1;
}

