#include "driver.h"
#include "dev.h"

#include "../memory/heap.h"
#include "../lib/assert.h"
#include "../lib/logging.h"

struct node {
    driver_t dr;
    struct node* next;
};

static struct node* head = NULL;


// alloc the structure and fill with
// basic info
// and put it in the list
static driver_t* push_driver_struct(
                        struct dev* dev
) {
    struct node* n = malloc(sizeof(struct node));

    n->dr.device = dev;
    n->dr.status = DRIVER_STATE_UNINITIALIZED;
    
    // append it to the list
    n->next = head;
    head = n;

    return &n->dr;
}

static void free_node(struct node* n) {
    assert(n != NULL);
    string_free(&n->dr.name);
    free(n);
}

// erase the last pushed driver
static void pop_driver_struct(void) {
    assert(head != NULL);
    struct node* old = head;
    head = head->next;

    free_node(old);
}

driver_t* driver_register_and_install(
                int (*install)(struct driver*), 
                struct dev* dev
) {
    driver_t* dr = push_driver_struct(dev);
    dr->install = install;

    if(install(dr)) {
        assert(dr->remove   != NULL);
        assert(dr->name.ptr != NULL);

        dev->driver = dr;

        log_debug("'%s' installed", dr->name);

        return dr;
    }
    else {
        pop_driver_struct();
        return NULL;
    }
}


void remove_all_drivers(void) {
    // iterate through the driver list
    for(struct node* node = head;
                     node != NULL;
                     ) 
    {
        log_debug("removing driver %s...", 
                    node->dr.name.ptr);
        node->dr.remove(&node->dr);

        struct node* next = node->next;
        free_node(node);
        
        node = next;
    }
}
