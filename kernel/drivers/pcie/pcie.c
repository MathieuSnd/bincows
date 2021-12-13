#include "pcie.h"
#include "scan.h"

#include "../../lib/logging.h"
#include "../../lib/assert.h"
#include "../../memory/paging.h"
#include "../../lib/string.h"


static struct pcie_dev* devices = NULL;
static unsigned n_devices = 0;
void pcie_init(void) {
    devices = pcie_scan(&n_devices);
}


void pcie_init_devices(void) {
}