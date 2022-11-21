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


void shm_cleanup(void) {
    if(shms)
        free(shms);
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
        PRESENT_ENTRY | PL_XD | PL_RW
        // @todo this introduces a security issue:
        // PL_US should be put so that user
        // cannot access this range yet
    );
/*
    unmap_pages()
    // @todo
*/
    uint64_t pd = temp_pd();

    temp_release();

    return pd;
}



struct shm_instance* shm_create(size_t initial_size) {

    if(initial_size == 0 || initial_size > SHM_SIZE_MAX)
        return NULL;

    uint64_t rf = get_rflags();
    _cli();
    struct shm* ent = insert_shm();

    static shmid_t cur_id = 1;

    ent->id = cur_id++;
    ent->n_insts = 1;
    ent->size = initial_size;

    struct shm_instance* inst = malloc(sizeof(struct shm_instance*));

    shm_alloc(initial_size);


    set_rflags(rf);
    return inst;
}
