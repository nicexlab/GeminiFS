#ifndef __linux__
#error "Must compile for Linux"
#endif

#include <nvm_types.h>
#include <nvm_ctrl.h>
#include <nvm_util.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include "linux/map.h"
#include "linux/ioctl.h"
#include "lib_ctrl.h"
#include "dprintf.h"
#include <nvm_queue.h>


/*
 * Unmap controller memory and close file descriptor.
 */
static void release_device(struct device* dev)
{

    close(dev->fd_control);
    close(dev->fd_dev);
    free(dev);
}

/*
 * Call kernel module ioctl and map memory for DMA.
 */
static int ioctl_map(const struct device* dev, const struct va_range* va, uint64_t* ioaddrs)
{
    const struct ioctl_mapping* m = _nvm_container_of(va, struct ioctl_mapping, range);
    enum nvm_ioctl_type type;

    switch (m->type)
    {
        case MAP_TYPE_API:
        case MAP_TYPE_HOST:
            type = NVM_MAP_HOST_MEMORY;
            break;
        case MAP_TYPE_CUDA:
            type = NVM_MAP_DEVICE_MEMORY;
            break;
        default:
            dprintf("Unknown memory type in map for device");
            return EINVAL;
    }

    struct nvm_ioctl_map request = {
        .vaddr_start = (uintptr_t) m->buffer,
        .n_pages = va->n_pages,
        .ioaddrs = ioaddrs,
        .ioq_idx = m->ioq_idx,
        .is_cq = m->is_cq
    };

    int err = ioctl(dev->fd_dev, type, &request);
    if (err < 0)
    {
        dprintf("Page mapping kernel request failed (ptr=%p, n_pages=%zu, tyep=%d): %s\n", 
                m->buffer, va->n_pages, m->type, strerror(errno));
        return errno;
    }
    
    return 0;
}
struct controller* ctrl_to_controller(nvm_ctrl_t* ctrl)
{
    struct controller* controll;
    controll = _nvm_container_of(ctrl, struct controller, handle);
    if(controll)
        return controll;
    else
        return NULL;
}

int ioctl_set_qnum(nvm_ctrl_t* ctrl, int ioq_num)
{
    struct controller* container;
    container  = ctrl_to_controller(ctrl);
    if(container==NULL)
    {
        printf("container error!\n");
        return -1;
    }

    struct nvm_ioctl_map request = {
        .ioq_idx = ioq_num,
    };
    int err = ioctl(container->device->fd_dev, NVM_SET_IOQ_NUM, &request);
    if (err < 0)
    {
        printf("ioctl_set_qnum err is %d\n",err);
        return errno;
    }
    
    return 0;
}

void ioctl_clear_qnum(nvm_ctrl_t* ctrl)
{
    struct controller* container;
    container  = ctrl_to_controller(ctrl);
    if(container==NULL)
    {
        printf("container error!\n");
    }
    ioctl(container->device->fd_dev, NVM_CLEAR_IOQ_NUM, NULL);
}

int ioctl_use_userioq(nvm_ctrl_t* ctrl, int use)
{
    int err;
    struct controller* container;
    container  = ctrl_to_controller(ctrl);
    if(container==NULL)
    {
        printf("container error!\n");
        return -1;
    }
    struct nvm_ioctl_map request = {
        .ioq_idx = use,
    };
    err = ioctl(container->device->fd_dev, NVM_SET_SHARE_REG, &request);
    if (err < 0)
    {
        printf("ioctl_set_qnum err is %d\n",err);
        return errno;
    }
    
    return 0;
}

int ioctl_get_dev_info_host(nvm_ctrl_t* ctrl, struct disk* d)
{
    int err;
    struct controller* container;
    container  = ctrl_to_controller(ctrl);
    if(container==NULL)
    {
        printf("container error!\n");
        return -1;
    }
    struct nvm_ioctl_dev dev_info;
    err = ioctl(container->device->fd_dev, NVM_GET_DEV_INFO, &dev_info);
    if (err < 0)
    {
        printf("ioctl_get_dev_info err is %d\n",err);
        return errno;
    }
    ctrl->start_cq_idx = dev_info.start_cq_idx;
    ctrl->nr_user_q = dev_info.nr_user_q;
    d->max_data_size = dev_info.max_data_size *512; //get the ctrl->max_hw_sectors from kernel    
    d->block_size = dev_info.block_size; // ns->lba_shift
    return 0;
}

int init_userioq(nvm_ctrl_t* ctrl, struct disk* d)
{
    int err,i,count;
    err = ioctl_get_dev_info_host(ctrl,d);
    if(err)
    {
        return -1;
    }
    printf("idx start is %u, dbstrd is %u, nr user q is %u\n",ctrl->start_cq_idx,ctrl->dstrd,ctrl->nr_user_q);
    if(ctrl->nr_user_q > ctrl->cq_num)
    {
        return -1;
    }
    for(i = 0; i < ctrl->nr_user_q; i++)
    {
        
        nvm_queue_clear(&ctrl->queues[i].queue,ctrl,true,i+ctrl->start_cq_idx,ctrl->qs,1,ctrl->queues[i].qmem.buffer,ctrl->queues[i].qmem.dma->ioaddrs[0]);
    }
    count = 0;
    for(i = ctrl->cq_num; i < ctrl->cq_num + ctrl->nr_user_q; i++)
    {
        
        nvm_queue_clear(&ctrl->queues[i].queue,ctrl,false,count+ctrl->start_cq_idx,ctrl->qs,1,ctrl->queues[i].qmem.buffer,ctrl->queues[i].qmem.dma->ioaddrs[0]);
        count++;
    }
        

    return 0;
}


int ioctl_reg_nvme(nvm_ctrl_t* ctrl, int reg)
{
    int err;
    struct controller* container;
    container  = ctrl_to_controller(ctrl);
    if(container==NULL)
    {
        printf("container error!\n");
        return -1;
    }
    
    if(reg)
        err = ioctl(container->device->fd_control, SNVM_REGISTER_DRIVER, NULL);
    else
        err = ioctl(container->device->fd_control, SNVM_UNREGISTER_DRIVER, NULL);
    if (err < 0)
    {
        printf("ioctl_req_nvme err is %d , reg is %d\n",err,reg);
        return errno;
    }
    
    return 0;
}


int ioctl_rebind_nvme(nvm_ctrl_t* ctrl, struct pci_device_addr device_addr, int bind){
    int err;
    struct controller* container;
    container  = ctrl_to_controller(ctrl);
    if(container==NULL)
    {
        printf("container error!\n");
        return -1;
    }
    if(bind){
        err = ioctl(container->device->fd_control, SNVM_DEVICE_BIND, &device_addr);
    }else{
        err = ioctl(container->device->fd_control, SNVM_DEVICE_UNBIND, &device_addr);
    }

    if (err < 0)
    {
        printf("ioctl_rebind_nvme err is %d , reg is %d\n",err,bind);
        return errno;
    }
    return 0;
}

int ioctl_set_cdev(int fd_control, struct pci_device_addr device_addr, int bind){
    int err;

    if(bind){
        err = ioctl(fd_control, SNVM_SET_CDEV, &device_addr);
    }else{
        err = ioctl(fd_control, SNVM_CLEAR_CDEV, &device_addr);
    }

    if (err < 0)
    {
        printf("ioctl_set_cdev err is %d , reg is %d\n",err,bind);
        return errno;
    }
    return 0;
}

/*
 * Call kernel module ioctl and unmap memory.
 */
static void ioctl_unmap(const struct device* dev, const struct va_range* va)
{
    const struct ioctl_mapping* m = _nvm_container_of(va, struct ioctl_mapping, range);
    uint64_t addr = (uintptr_t) m->buffer;
    unsigned int unmap_type;
    if(m->type == MAP_TYPE_HOST)
        unmap_type = NVM_UNMAP_HOST_MEMORY;
    else if(m->type == MAP_TYPE_CUDA)
        unmap_type = NVM_UNMAP_DEVICE_MEMORY;
    else
    {
        printf("Page unmapping kernel type error,type is %lu\n",m->type);
        return;
    }
    int err = ioctl(dev->fd_dev, unmap_type, &addr);
    if (err < 0)
    {
        dprintf("Page unmapping kernel request failed: %s\n", strerror(errno));
    }
}


int nvm_ctrl_init(nvm_ctrl_t** ctrl, int snvme_c_fd, int snvme_d_fd)
{
    int err;
    struct device* dev;


    const struct device_ops ops = {
        .release_device = &release_device,
        .map_range = &ioctl_map,
        .unmap_range = &ioctl_unmap
    };

    *ctrl = NULL;

    dev = (struct device*) malloc(sizeof(struct device));
    if (dev == NULL)
    {
        dprintf("Failed to allocate device handle: %s\n", strerror(errno));
        return ENOMEM;
    }

    dev->fd_control = dup(snvme_c_fd);
    if (dev->fd_control < 0)
    {
        free(dev);
        dprintf("Could not duplicate file descriptor: %s\n", strerror(errno));
        return errno;
    }
    
    dev->fd_dev = dup(snvme_d_fd);
    dprintf("fd_dev %d\n", dev->fd_dev);
    if (dev->fd_control < 0)
    {
        close(dev->fd_control);
        free(dev);
        dprintf("Could not duplicate file descriptor: %s\n", strerror(errno));
        return errno;
    }
    

    err = fcntl(dev->fd_control, F_SETFD, O_RDWR);
    if (err == -1)
    {
        close(dev->fd_control);
        close(dev->fd_dev);
        free(dev);
        dprintf("Failed to set file descriptor control: %s\n", strerror(errno));
        return errno;
    }
    err = fcntl(dev->fd_dev, F_SETFD, O_RDWR);
    if (err == -1)
    {
        close(dev->fd_control);
        close(dev->fd_dev);
        free(dev);
        dprintf("Failed to set file descriptor control: %s\n", strerror(errno));
        return errno;
    }

    void* mm_ptr = mmap(NULL, NVM_CTRL_MEM_MINSIZE, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FILE|MAP_LOCKED, dev->fd_dev, 0);
    if (mm_ptr == NULL)
    {
        close(dev->fd_control);
        close(dev->fd_dev);
        free(dev);
        dprintf("Failed to map device memory: %s\n", strerror(errno));
        return err;
    }    
    printf("mmap is %lx\n",mm_ptr);

    err = _nvm_ctrl_init(ctrl, dev, &ops, DEVICE_TYPE_IOCTL,mm_ptr,NVM_CTRL_MEM_MINSIZE);
    if (err != 0)
    {
        release_device(dev);
        return err;
    }
    return 0;
}

