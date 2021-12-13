#include "pcie.h"
#include "../../lib/logging.h"
#include "../../lib/dump.h"
#include "../../lib/assert.h"
#include "../../memory/heap.h"
#include "../../memory/paging.h"
#include "../../lib/string.h"


static struct pcie_device* devices = NULL;

void pcie_init(void) {
    devices = pcie_scan();
}


void pcie_init_devices(void) {
}