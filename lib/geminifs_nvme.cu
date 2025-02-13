#include <nvm_types.h>
#include <nvm_util.h>
#include <nvm_cmd.h>
#include <ctrl.h>
#include "geminifs_api.h"

static Controller *ctrl;

void *host_open_all(
    const char *snvme_control_path,
    const char *snvme_path,
    const char *nvme_dev_path,
    const char *mount_path,
    struct pci_device_addr dev_addr,
    uint32_t ns_id,
    uint64_t queueDepth,
    uint64_t numQueues) {
    ctrl = new Controller(
        (char *)snvme_control_path,
        (char *)snvme_path,
        (char *)nvme_dev_path,
        (char *)mount_path,
        dev_addr, ns_id, queueDepth, numQueues);

    return (void *)ctrl;
}

void
host_close_all() {
    delete ctrl;
}