#ifndef __NVM_INTERNAL_LINUX_IOCTL_H__
#define __NVM_INTERNAL_LINUX_IOCTL_H__
#ifdef __linux__

#include <linux/types.h>
#include <asm/ioctl.h>

#define NVM_IOCTL_TYPE          0x80



/* Memory map request */
struct nvm_ioctl_map
{
    uint64_t    vaddr_start;
    size_t      n_pages;
    uint64_t*   ioaddrs;
    int ioq_idx; // if the ioq_idx > 0, indicate the map is a IOQ
    int is_cq; // cq = 1 sq = 0
};

struct nvm_ioctl_dev
{
    uint32_t    nr_user_q;
    uint32_t   start_cq_idx;
    uint8_t     dstrd; 
    size_t      max_data_size; //get the ctrl->max_hw_sectors from kernel
    size_t      block_size;    // ns->lba_shift
};

// struct pci_device_addr; // Removed redundant forward declaration

/* Supported operations */
enum nvm_ioctl_type
{
    NVM_MAP_HOST_MEMORY         = _IOW(NVM_IOCTL_TYPE, 1, struct nvm_ioctl_map),
    NVM_MAP_DEVICE_MEMORY       = _IOW(NVM_IOCTL_TYPE, 2, struct nvm_ioctl_map),
    NVM_UNMAP_HOST_MEMORY            = _IOW(NVM_IOCTL_TYPE, 3, uint64_t),
    NVM_UNMAP_DEVICE_MEMORY            = _IOW(NVM_IOCTL_TYPE, 4, uint64_t),
    NVM_SET_IOQ_NUM            = _IOW(NVM_IOCTL_TYPE, 5, uint64_t),
    NVM_SET_SHARE_REG            = _IOW(NVM_IOCTL_TYPE, 6, uint64_t),
    NVM_GET_DEV_INFO             = _IOR(NVM_IOCTL_TYPE, 7, struct nvm_ioctl_dev),   
    NVM_CLEAR_IOQ_NUM            = _IOW(NVM_IOCTL_TYPE, 8, uint64_t)
};

struct pci_device_addr{ // Removed redundant definition
    int bus;
    int slot;
    int func;
};

// snvm_ctrl_ioctl_type
enum snvm_ctrl_ioctl_type{
    SNVM_REGISTER_DRIVER    = _IO('N', 0x0),
    SNVM_UNREGISTER_DRIVER  = _IO('N', 0x1),
    SNVM_DEVICE_BIND        = _IOW('N', 0x2, struct pci_device_addr),
    SNVM_DEVICE_UNBIND      = _IOW('N', 0x3, struct pci_device_addr),
    SNVM_SET_CDEV           = _IOW('N', 0x4, struct pci_device_addr),
    SNVM_CLEAR_CDEV         = _IOW('N', 0x5, struct pci_device_addr)
};


/* SNVME initiazation process*/
/*
1. Use NVM_SET_IOQ_NUM set IO queues num
2. Use NVM_MAP_HOST_MEMORY/NVM_MAP_DEVICE_MEMORY reg enough dma address on /dev/snvme, must equal to IO queues num 
3. Use NVM_SET_SHARE_REG tp set flag to enable using user provided dma address, nvme_probe will check it during register
4. Use SNVM_REGISTER_DRIVER to control /dev/snvme_control register the nvme
*/
#endif /* __linux__ */
#endif /* __NVM_INTERNAL_LINUX_IOCTL_H__ */
