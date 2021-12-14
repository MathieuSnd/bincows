#include <stdint.h>
#include <stddef.h>

#include "heap.h"
#include "../lib/sprintf.h"
#include "../memory/vmap.h"
#include "../memory/paging.h"
#include "../lib/logging.h"
#include "../lib/assert.h"
#include "../lib/string.h"


#define MIN_EXPAND_SIZE 1024
#define MIN_SEGMENT_SIZE 32

#define MAX(X,Y) (X >= Y ? X : Y)

typedef struct seg_header {
    struct seg_header *next;
    uint32_t size;

// boolean variable
    uint32_t free;
} seg_header;


// assert that the header is 8-byte alligned
// this is important so that every allocation
// is 8-byte alligned too
static_assert(sizeof(seg_header) % 8 == 0);

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

static void *heap_begin = (void *)KERNEL_HEAP_BEGIN;
static size_t heap_size = 0;

// sum of the available heap ranges, 
// without the last free segment:
// indice of how fragmented
// the heap is
// TODO: use this to unfragment the whole
// heap when this number is too big

//static size_t fragmented_available_size = 0;

static seg_header* current_segment = NULL;

/**
 * expand the heap by size bytes
 */
static void expand_heap(size_t size) {
    size_t new_heap_pages_size = (heap_size + size + 0xfff) >> 12;
    size_t old_heap_pages_size = (heap_size        + 0xfff) >> 12;


// alloc extra pages if needed
    if(new_heap_pages_size != old_heap_pages_size) {
        alloc_pages(
            heap_begin + (old_heap_pages_size << 12),
            new_heap_pages_size - old_heap_pages_size,
            PRESENT_ENTRY | PL_XD // execute disable pages
        );
    }

// create a new segment in the extra space
    seg_header* new_segment = heap_begin + heap_size;


    new_segment->next = current_segment;
    new_segment->size = size - sizeof(seg_header);
    new_segment->free = 1;

    current_segment = new_segment;


    heap_size += size;

    log_debug("kernel heap extended to %lu KB", heap_size / 1024);
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
    assert(0);
    __builtin_unreachable();
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


void heap_init(void) {

    log_debug("init kernel heap...");

    expand_heap(MIN_EXPAND_SIZE);
}


void* __attribute__((noinline)) malloc(size_t size) {
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

        assert(is_kernel_memory((uint64_t)seg));

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

        return (void *)seg + sizeof(seg_header);
    }

    // no available segment in the heap
    // let's expand

    expand_heap(MAX(size+sizeof(seg_header), MIN_EXPAND_SIZE));
// retrty now that we are sure that the memory is avaiable
    return malloc(size);
}

static void realloc_shrink(seg_header* header, size_t size) {
    // check if we can actually free some memory
    if(header->size - size >= sizeof(seg_header) + MIN_SEGMENT_SIZE) {
        split_segment(
            find_pred(header), // we need to find the preceding
                                // node in order to split it
            header,
            size
        );
    }
    else {
        // the difference is not big enough
        // to actually free anything
        // let's not even change the size
        // in the structure
        // so that if we do:
        // realloc(ptr, size-X);
        // realloc(ptr, size);
        // no reallocation will be done
    }
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
    if(size <= header->size) {// no need to move
        realloc_shrink(header, size);
        return ptr;
    }
    else {
        // maybe need reallocation
        
        // mark the node free
        header->free = 1;
        unsigned old_size = header->size;

        // hope for the node to merge with another
        defragment();


        // check the size again
        if(header->size >= size) {
            // we have enough space now!
            header->free = 0;
            realloc_shrink(header, size);
            return ptr;
        }
        else {
            // still not enough space
            // we have to copy the whole thing
            void* new_ptr = malloc(size);
            memcpy(new_ptr, ptr, old_size);
            
            return new_ptr;
        }
    }
}


// O(1) free
void __attribute__((noinline))  free(void *ptr) {
    seg_header* header = ptr - sizeof(seg_header);
    
    assert(header->free == 0);

    header->free = 1;

    static int i = 0;

    // defragment the heap every N frees
    if((i++ % 32) == 0)
        defragment();
}


#ifndef NDEBUG

void print_heap(void) {
    for(seg_header* seg = current_segment; 
                    seg != NULL;
                    seg = seg->next) {
        log_debug("%lx size=%x,free=%u", seg,seg->size, seg->free);
    }
}

void malloc_test(void) {

    void* arr[128];

    uint64_t size = 5;

    for(int j = 0; j < 4000; j++) {
        for(int i = 0; i < 128; i++) {
            arr[i] = malloc(size % (1024*1024));
            size = (16807 * size) % ((1lu << 31) - 1);
        }
        
        for(int i = 0; i < 128; i++)
            free(arr[i]);
    }
}
#endif
