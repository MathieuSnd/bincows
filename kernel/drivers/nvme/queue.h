#pragma once

#include <stdint.h>

#include "../../lib/assert.h"


struct queue {
    void* __restrict__ base; // vaddr
    uint16_t size;
    uint16_t element_size;
    uint16_t head, // consumer
             tail; // producer
    
    uint16_t id;
};

struct queue_pair {
    struct queue sq, cq;
};

struct driver;

// alloc with pmm
// ! only 1 page max !
struct queue create_queue(
                    unsigned size, 
                    unsigned element_size);

void free_queue(struct queue*);


static inline void* queue_tail(struct queue* queue) {
    return queue->base 
         + queue->tail
         * queue->element_size;
}


static inline void* queue_head(struct queue* queue) {
    return queue->base 
         + queue->tail
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

