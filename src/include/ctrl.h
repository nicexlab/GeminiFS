
#ifndef __BENCHMARK_CTRL_H__
#define __BENCHMARK_CTRL_H__

// #ifndef __CUDACC__
// #define __device__
// #define __host__
// #endif

#include <cstdint>
#include "linux/ioctl.h"
#include "nvm_types.h"
#include "nvm_ctrl.h"
#include "nvm_error.h"
#include "nvm_dma.h"
#include <string>
#include <stdexcept>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <simt/atomic>
#include "../file.h"

#define MAX_QUEUES 1024
#define NVM_CTRL_IOQ_MINNUM    64

static void remove_buffer(struct buffer* b)
{
    nvm_dma_unmap(b->dma);
    free(b->buffer);
}

static void remove_queue(struct queue* q)
{
    remove_buffer(&q->qmem);
}

static void remove_queues(struct queue* queues, uint16_t n_queues)
{
    uint16_t i;

    if (queues != NULL)
    {

        for (i = 0; i < n_queues; i++)
        {
            remove_queue(&queues[i]);
        }

        free(queues);
    }
}

static int create_buffer(struct buffer* b, nvm_ctrl_t* ctrl, size_t size,int is_cq, int ioq_idx)
{
    int status;

    status = posix_memalign(&b->buffer, ctrl->page_size, size);
    if (status != 0)
    {
        fprintf(stderr, "Failed to allocate memory: %s\n", strerror(status));
        return status;
    }

    status = nvm_dma_map_host(&b->dma, ctrl, b->buffer, size, is_cq, ioq_idx);

    if (!nvm_ok(status))
    {
        free(b->buffer);
        fprintf(stderr, "Failed to create local segment: %s\n", nvm_strerror(status));
        return status;
    }

    memset(b->dma->vaddr, 0, b->dma->page_size * b->dma->n_ioaddrs);

    return 0;
}

static int create_queue(struct queue* q, nvm_ctrl_t* ctrl, const struct queue* cq, uint16_t qno)
{
    int status;

    int is_cq;
    size_t qmem_size;
    
    is_cq = 1;
    if (cq != NULL)
    {
        is_cq = 0;
        qmem_size =  ctrl->qs * sizeof(nvm_cmd_t);
    }
    else
        qmem_size =  ctrl->qs * sizeof(nvm_cpl_t);
    
    status = create_buffer(&q->qmem, ctrl, qmem_size,is_cq,qno);
    if (!nvm_ok(status))
    {
        return status;
    }

    if (!nvm_ok(status))
    {
        remove_buffer(&q->qmem);
        fprintf(stderr, "Failed to create queue: %s\n", nvm_strerror(status));
        return status;
    }

    q->counter = 0;
    return 0;
}


static int request_queues(nvm_ctrl_t* ctrl, struct queue** queues)
{
    struct queue* q;
    *queues = NULL;
    uint16_t i;
    int status;
    status = ioctl_set_qnum(ctrl, ctrl->cq_num+ctrl->sq_num);
    if (status != 0)
    {
    
        return status;
    }
    // Allocate queue descriptors
    q = (queue *)calloc(ctrl->cq_num+ctrl->sq_num, sizeof(struct queue));
    if (q == NULL)
    {
        fprintf(stderr, "Failed to allocate queues: %s\n", strerror(errno));
        return ENOMEM;
    }

    // Create completion queue
    for (i = 0; i < ctrl->cq_num; ++i)
    {
        status = create_queue(&q[i], ctrl, NULL, i);
        if (status != 0)
        {
            free(q);
            return status;
        }
    }


    // Create submission queues
    for (i = 0; i < ctrl->sq_num; ++i)
    {
        status = create_queue(&q[i + ctrl->cq_num], ctrl, &q[0], i);
        if (status != 0)
        {
            remove_queues(q, i);
            return status;
        }
    }
    printf("request_queues success\n");
    *queues = q;
    return status;
}

struct Controller
{
    nvm_ctrl_t*             ctrl;
    struct nvm_ctrl_info    info;
    struct nvm_ns_info      ns;
    struct disk             disk;
    uint16_t                n_sqs;
    uint16_t                n_cqs;
    uint16_t                n_qps;
    uint16_t                n_user_qps;


    uint32_t page_size;
    uint32_t blk_size;
    uint32_t blk_size_log;
    char* dev_path;
    char* dev_mount_path;

    void* d_ctrl_ptr;


    Controller::Controller(const char* snvme_control_path, 
                           const char* snvme_path, 
                           char* nvme_dev_path,
                           char* mount_path,
                           struct pci_device_addr dev_addr,
                           uint32_t ns_id,
                           uint64_t queueDepth, 
                           uint64_t numQueues);
    ~Controller();
};



using error = std::runtime_error;
using std::string;


inline Controller::Controller(const char* snvme_control_path, 
                                const char* snvme_path, 
                                char* nvme_dev_path,
                                char* mount_path,
                                struct pci_device_addr dev_addr,
                                uint32_t ns_id,
                                uint64_t queueDepth, 
                                uint64_t numQueues)
    : ctrl(nullptr), dev_path(nvme_dev_path), dev_mount_path(mount_path)
{
    int status;
    int snvme_c_fd = open(snvme_control_path, O_RDWR);
    if (snvme_c_fd < 0)
    {
        throw error(string("Failed to open descriptor: ") + strerror(errno));
    }

    status = ioctl_set_cdev(snvme_c_fd, dev_addr, 1);
    if (status < 0)
    {
        throw error(string("Failed to create device file: ") + strerror(errno));
    }

    
    int snvme_d_fd = open(snvme_path, O_RDWR);
    if (snvme_d_fd < 0)
    {
        throw error(string("Failed to open descriptor: ") + strerror(errno));
    }

    status = nvm_ctrl_init(&ctrl, snvme_c_fd,snvme_d_fd);
    if (!nvm_ok(status))
    {
        throw error(string("Failed to get controller reference: ") + nvm_strerror(status));
    }

    this->ctrl->device_addr = dev_addr;

    close(snvme_c_fd);
    close(snvme_d_fd);
    uint16_t max_queue = 75; 
    max_queue = std::min(max_queue, (uint16_t)numQueues);
    ctrl->cq_num = max_queue;
    ctrl->sq_num = max_queue;
    ctrl->qs = queueDepth;

    status = request_queues(ctrl, &ctrl->queues);
    if (!nvm_ok(status))
    {
        throw error(string("Failed to request queues: ") + nvm_strerror(status));
    }
    status =  ioctl_use_userioq(ctrl,1);
    if (!nvm_ok(status))
    {
        throw error(string("Failed to use user ioq: ") + nvm_strerror(status));
    }

    status = ioctl_rebind_nvme(ctrl, dev_addr, 1);
    if (!nvm_ok(status))
    {
        throw error(string("Failed to rebind nvme: ") + nvm_strerror(status));
    }
    disk.ns_id = ns_id;
    disk.page_size = ctrl->page_size;
    sleep(3); // wait for rebind device
    status = init_userioq(ctrl,&disk);
    if (!nvm_ok(status))
    {
        throw error(string("Failed to init user ioq: ") + nvm_strerror(status));
    }
    Host_file_system_int(dev_path,dev_mount_path);
}



inline Controller::~Controller()
{
    int ret = Host_file_system_exit(dev_path);
    if(ret < 0)
        exit(-1);
    printf("Controller realease\n");
    remove_queues(ctrl->queues, ctrl->cq_num + ctrl->sq_num);
    nvm_ctrl_free(ctrl);

}  


#endif
