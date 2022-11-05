#include <lai/core.h>
#include "../lib/assert.h"

void acpi_init(void) {

    // LAI scan
    lai_create_namespace();
    
    lai_nsnode_t* root = lai_ns_get_root();

    assert(root);
}