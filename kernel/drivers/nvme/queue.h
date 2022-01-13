#pragma once

#include <stdint.h>

#include "../../lib/assert.h"


struct queue {
    volatile void*     base; // vaddr
    uint16_t           size;
    uint16_t           element_size;
    volatile uint16_t  head; // consumer
    volatile uint16_t  tail; // producer
    
    uint16_t           id;
};

// sizes must be powers of two
// because admin_completion_counter 
// can overflow.
struct queue_pair {
    struct queue sq, cq;

    // used to calculate the phase bits
    // and therefore dectect new entries 
    // form the controller
    unsigned completion_counter;
};

struct driver;

// alloc with pmm
// ! only 1 page max !
struct queue create_queue(
                    unsigned size, 
                    unsigned element_size,
                    unsigned id);

void free_queue(struct queue*);


static inline volatile void* queue_tail_ptr(struct queue* queue) {
    return queue->base 
         + queue->tail
         * queue->element_size;
}


static inline volatile void* queue_head_ptr(struct queue* queue) {
    return queue->base 
         + queue->head
         * queue->element_size;
}


// return the following index in the circular buffer
static inline unsigned next_index(
            struct queue* queue,
            uint16_t      index
) {
    return (index + 1) % queue->size;
}

static inline int queue_full(struct queue* queue) {
    return next_index(queue, queue->tail) == queue->head;
}

static inline int queue_empty(struct queue* queue) {
    return queue->tail == queue->head;
}


// advance tail pointer and return it
static inline uint16_t queue_produce(struct queue* queue) {
    assert(! queue_full(queue));

    return queue->tail = next_index(queue, queue->tail);
}

// advance head pointer and return it
static inline uint16_t queue_consume(struct queue* queue) {
    assert(! queue_empty(queue));

    return queue->head = next_index(queue, queue->head);
}

// return the predicted bit phase of the tail pointer
// of the completion queue
static inline int cq_tail_phase(struct queue_pair* queues) {
    return (queues->completion_counter 
            / queues->cq.size) % 2;
}

// update the completion queue, by checking the phase
// bits of the entries and adding those to the queue 
// structure
// and update the submission queue (mark as consumed the 
// entries that are produced in the completion queue)
void update_queues(struct queue_pair* queues);