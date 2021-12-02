#include "pcie.h"
#include "../debug/logging.h"
#include "../debug/dump.h"

struct PCIE_Descriptor pcie_descriptor = {0};

void pcie_init(void) {
    klog_debug("init pcie...");
    klog_debug("%d", pcie_descriptor.size);
    
    dump(
        pcie_descriptor.array, 
        pcie_descriptor.size * sizeof(struct PCIE_segment_group_descriptor),
        sizeof(struct PCIE_segment_group_descriptor),
        DUMP_HEX8
    );
}