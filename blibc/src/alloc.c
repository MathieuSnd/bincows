#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>


#define MIN_EXPAND_SIZE 4096
#define MIN_SEGMENT_SIZE 32

#define MAX(X,Y) (X >= Y ? X : Y)

typedef struct seg_header {
    struct seg_header *next;
    uint32_t size;

// boolean variable
    uint32_t free;
} seg_header;

#define assert(X)
#define log_heap(X,Y)

// assert that the header is 8-byte alligned
// this is important so that every allocation
// is 8-byte alligned too
_Static_assert(sizeof(seg_header) % 8 == 0, "seg_header must be 8-byte alligned");

// segment headers represent a linked list
// going downward in address space

/**
 * 0  | node0
 *    | ////
 *    | ////
 *    | ////
 *    | node1
 *    | ////
 *    | ////
 *    | ////
 *      ...
 *    | nodeN 
 *    | free
 *    | free
 *    | free
 * BRK  ---- 
 * 
 * node n -> node n-1
 * 
 * 
 * split:
 *      node n+1 -> node n
 * 
 *      node n+1 -> new_node
 *      new_node -> node n
 *      new_node.size = node n.size - SIZE - sizeof(node)
 * 
 */

// we need to get the heap base address
// with a sbrk call to set this variable
// at runtime
// we could do that with a crt0.S file
static void *heap_begin = NULL;

static size_t heap_size = 0;
static size_t n_allocations = 0;
// sum of the available heap ranges, 
// without the last free segment:
// indice of how fragmented
// the heap is
// TODO: use this to unfragment the whole
// heap when this number is too big

//static size_t fragmented_available_size = 0;

static seg_header* current_segment = NULL;

#ifndef NDEBUG
static void heap_assert_seg(const seg_header* const seg) {
    assert(seg);
    if((uint64_t)seg < heap_begin || 
        (uint64_t)seg >= heap_begin + heap_size) {
        printf("heap_assert_seg error: seg=%lx\n", seg);
        printf("heap_assert_seg(...): arg is not in heap\n");
        exit(1);
    }

    if((uint64_t)seg+seg->size > heap_begin + heap_size) {
        printf("heap_assert_seg error: seg=%lx = {.next=%lx, .size=%lx, .free=%lx}\n", 
                    seg, seg->next, seg->size, seg->free);
        printf("heap_assert_seg(...): arg is not in heap\n");
        exit(1);
    }
    if((seg->free & ~1u) != 0) {
        printf("heap_assert_seg error: seg=%lx = {.next=%lx, .size=%lx, .free=%lx}\n", 
                    seg, seg->next, seg->size, seg->free);
        printf("heap_assert_seg(...): bad header (invalid free value)\n");
        exit(1);
    }
    
    if(seg->next != NULL && ((uint64_t)seg->next < heap_begin 
                || (uint64_t)seg->next >= heap_begin + heap_size)) {
        printf("heap_assert_seg error: seg=%lx = {.next=%lx, .size=%lx, .free=%lx}\n", 
                    seg, seg->next, seg->size, seg->free);
        printf("heap_assert_seg(...): bad header (invalid next value)\n");
        exit(1);
    }
}

#else
#define heap_assert_seg(X)
#endif


static void defragment(void);


/**
 * expand the heap by size bytes
 */
static int expand_heap(size_t size) {

    if(sbrk(size) == (void*)-1) {
        // out of memory :(
        return -1;
    }

// create a new segment in the extra space
    seg_header* new_segment = heap_begin + heap_size;


    new_segment->next = current_segment;
    new_segment->size = size - sizeof(seg_header);
    new_segment->free = 1;

    current_segment = new_segment;


    heap_size += size;

    return 0;
}


// O(1) free
void __attribute__((noinline)) free(void *ptr) {
    log_heap("free(%lx)", ptr);

    seg_header* header = ptr - sizeof(seg_header);

    heap_assert_seg(header);

    assert(header->free == 0);


    header->free = 1;

    static int i = 0;

    // defragment the heap every N frees
    if((i++ % 32) == 0)
        defragment();

    n_allocations--;
}



/**
 * current_segment shouldn't be NULL
 * 
 */
static void defragment(void) {
    assert(current_segment != NULL);
    
    seg_header* pred2 = NULL;
    seg_header* pred = current_segment;

    for(seg_header* seg = current_segment->next; 
                        seg != NULL;
                        ) {
            heap_assert_seg(seg);
 /**
  *   ||   seg   ||   pred   | 
  *             ||
  *             \/  
  *   ||        SEG          |
  * 
  * 
  */
            // we can merge these two nodes
            if(seg->free && pred->free) {
                if(!pred2)
                    // pred is the head
                    current_segment = seg;
                else 
                    pred2->next = seg;
                
                seg->size += sizeof(seg_header) + pred->size;

                // pred2 doesn't change!
                pred = seg;
                seg  = seg->next;
                continue;
            }
            else {
                pred2 = pred;
                pred = seg;
                seg = seg->next;
            }
        }
}

// return the node preceding the argument
// in the linked list
// O(n) sequential research
static seg_header* find_pred(seg_header* node) {

    for(seg_header* seg = current_segment; 
                    seg != NULL;
                    seg = seg->next) {
        if(seg->next == node)
            return node;
    }
    return NULL;
}

// try to merge the node with the next one
// we suppose that the argument is free
static inline void try_merge(seg_header* free_node) {
    assert(free_node->free);

    seg_header* next = free_node->next;

    if(!next)
        return;

/*
 * insert a new segment between to_split and pred
 * return the new segment
 *
 * |  next    | free_node |
 * | F R E E  |  F R E E  |
 * |   size1  |   size0   | 
 * 
 *            ||
 *            \/
 * 
 * |         next         |
 * |      F  R  E  E      |
 * |     size0 + size1    |
 *
**/
    // we can merge!
    if(next->free) {
        next->size += free_node->size;

        // if the argument is the head
        // of the list, the head should
        // become the 
        if(free_node == current_segment) {
            current_segment = next;
            return;
        }

        // if not, we must find the preceding
        // node in the linked list
        seg_header* seg = find_pred(free_node);

        // now make it point on the 
        // new merged node which is
        // the 'next' node
        seg->next = next;
    }
}



/*
 * insert a new segment between to_split and pred
 * return the new segment
 * |       to_split      |
 * |      F  R  E  E     |
 * |         size0       |
 *            ||
 *            \/
 *
 * | to_split |   new    |
 * | F R E E  | F R E E  |
 * |   size   |size0-size| 
**/
static seg_header* split_segment(seg_header* pred, seg_header* tosplit, size_t size) {
    seg_header* new_segment = (void*)tosplit + sizeof(seg_header) + size;

    if(pred != NULL)
        pred->next = new_segment;

    new_segment->next = tosplit;
    new_segment->free = 1;
    new_segment->size = tosplit->size           // size to split
                        - size                  // size reserved
                        - sizeof(seg_header);   // new segment header


    tosplit->size = size;

    return new_segment;
}



void* __attribute__((noinline)) malloc(size_t size) {
    
    if(heap_begin == NULL) {
        // initialize the heap
        heap_begin = sbrk(0);
        heap_size = 0;
    }

    //assert(current_segment->free == 1);

    // align the size to assure that 
    // the whole structure is alligned
    size = ((size + 7 ) / 8) * 8;
    if(size < MIN_SEGMENT_SIZE)
        size = MIN_SEGMENT_SIZE;

    // search for a big enough pool
    seg_header* seg  = current_segment;
    seg_header* pred = NULL;


    while(1) {
        if(seg == NULL) {
            break;
        }

        heap_assert_seg(seg);

        if(!seg->free || seg->size < size) {    
            // this segment is not right, check the next one
            pred = seg;
            
            // keep the preceding node in memory
            // we need it when spliting the current 
            // segment
            seg  = seg->next;
            
            continue;
        }

        // we found a satisfying segment!
        if(seg->size >= size + sizeof(seg_header) + MIN_SEGMENT_SIZE) {
            // we don't take the whole space

            // get the inserted segment
            seg_header* new_seg = split_segment(pred, seg, size);

            if(pred == NULL) {
                // seg == current_segment

                // the 'current' segment should
                // be the last in the list.
                // => advance the current one
                current_segment = new_seg;
            }
            else {
                // put the new node in the linked list
                pred->next = new_seg;
            }

            // mark seg as allocated,
            // leaving the new node free
        }
        // else, the segment is not big enough to
        // be splitted, we mark it allocated as is,
        // wasting a bit of memory
        
        seg->free = 0;
        
        // one allocation
        n_allocations++;

        log_heap(" --> %lx", (void*)seg+sizeof(seg_header));
        
        return (void *)seg + sizeof(seg_header);
    }

    // no available segment in the heap
    // let's expand

    if(expand_heap(MAX(size+sizeof(seg_header), MIN_EXPAND_SIZE)) == -1) {
        // out of memory
        return NULL;
    }

// retrty now that we are sure that the memory is avaiable
    return malloc(size);
}


// O(n) in case of freeing (size < oldsize)
void* realloc(void* ptr, size_t size) {
    if(ptr == NULL)
        return malloc(size);
    if(size == 0) {
        free(ptr);
        return NULL;
    }

    seg_header* header = ptr - sizeof(seg_header);

    uint32_t header_size = header->size;

    if(size < header_size) {
        // its not worth reallocating
        if(size > header_size / 2 
          || header_size-size < MIN_SEGMENT_SIZE * 2)
            return ptr;
        
    } 

    unsigned cpsize = header_size;
    if(cpsize > size)
        cpsize = size;

    void* new_ptr = malloc(size);
    memcpy(new_ptr, ptr, cpsize);
    
    free(ptr);
    // malloc increments the number of allocations
    // but we just reallocated
    //  n_allocations--;

    return new_ptr;
}


void * calloc(size_t n, size_t sz)
{
    void *ptr = malloc(n * sz);
    
    if (ptr == 0)
        return NULL;

    return memset(ptr, 0, n * sz);
}

