#include <nvm_types.h>
#include <nvm_ctrl.h>
#include <nvm_dma.h>
#include <nvm_aq.h>
#include <nvm_error.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>


#include <fcntl.h>
#include <unistd.h>

#include "get-offset/get-offset.h"
#include "integrity.h"
#include "read.h"
#include "../../src/file.h"

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

#define snvme_control_path "/dev/snvm_control"
#define snvme_path "/dev/csnvme1"
#define nvme_dev_path "/dev/snvme0n1"
#define snvme_helper_path "/dev/snvme_helper"
#define nvme_mount_path "/mnt/nvm_mount"
#define file_name "/mnt/nvm_mount/test.data"
#define nvme_pci_addr {0xc3, 0, 0}

int main(int argc, char** argv)
{
    nvm_ctrl_t* ctrl;
    int status,ret;
    struct disk disk;
    struct buffer buffer;
    int snvme_c_fd,snvme_d_fd;
    // Parse command line arguments
    int *buffer2;
    int read_bytes;
    read_bytes = 1024*64;
    void *buf__host = NULL;
    int *buf__host_int = NULL;
    int fd;
    struct queue_pair qp;
    struct file_info read_info;
    int snvme_helper_fd;
    struct nds_mapping mapping;
    uint64_t nvme_ofst;

    snvme_c_fd = open(snvme_control_path, O_RDWR);

    if (snvme_c_fd < 0)
    {

        fprintf(stderr, "Failed to open device file: %s\n", strerror(errno));
        exit(1);
    }

    // Get controller reference

    ret = ioctl_set_cdev(snvme_c_fd, nvme_pci_addr, 1);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to create device file: %s\n", strerror(errno));
        exit(1);
    }

    snvme_d_fd = open(snvme_path, O_RDWR);
    if (snvme_d_fd < 0)
    {
        fprintf(stderr, "Failed to open device file: %s\n", strerror(errno));
        exit(1);
    }

    status = nvm_ctrl_init(&ctrl, snvme_c_fd, snvme_d_fd);
    ctrl->device_addr = nvme_pci_addr;
    if (status != 0)
    {
        close(snvme_c_fd);
        close(snvme_d_fd);
        
        fprintf(stderr, "Failed to get controller reference: %s\n", strerror(status));
    }
    
    close(snvme_c_fd);
    close(snvme_d_fd);

    ctrl->cq_num = 16;
    ctrl->sq_num = 16;
    ctrl->qs = 1024;
    // Create queues
    status = request_queues(ctrl, &ctrl->queues);
    if (status != 0)
    {
        goto out;
    }
    status =  ioctl_use_userioq(ctrl,1);
    if (status != 0)
    {
        goto out;
    }
    /*Prepare Buffer for read/write, need convert vaddt to io addr*/

    status = create_buffer(&buffer, ctrl, 4096,0,-1);
    if (status != 0)
    {
        goto out;
    }

    status = ioctl_rebind_nvme(ctrl, nvme_pci_addr, 1);
    if (status != 0)
    {
        goto out;
    }

    disk.ns_id = 1;
    disk.page_size = ctrl->page_size;
    printf("page size is %lu\n",disk.page_size);
    sleep(5);

    status =  init_userioq(ctrl,&disk);
    if (status != 0)
    {
        goto out;
    }
    printf("disk block size is %u, max data size is %u\n",disk.block_size,disk.max_data_size);
    Host_file_system_int(nvme_dev_path,nvme_mount_path);
    fd = open(file_name, O_RDWR| O_CREAT | O_DIRECT , S_IRUSR | S_IWUSR);
    if (fd < 0) {
        perror("Failed to open file");
        goto out;
    }

    ret = posix_memalign(&buf__host, 4096, read_bytes);
    assert(ret==0);
    assert(0 == ftruncate(fd, read_bytes*16));
    
    buf__host_int = (int*)buf__host;
    for (size_t i = 0; i < read_bytes / sizeof(int); i++)
        buf__host_int[i] = i;
    snvme_helper_fd = open(snvme_helper_path, O_RDWR);
    if (snvme_helper_fd < 0) {
        perror("Failed to open snvme_helper_fd");
        assert(0);
    }
    assert(read_bytes == pwrite(fd, buf__host_int, read_bytes,read_bytes));
    
    fsync(fd);   
    
    mapping.file_fd = fd;
    mapping.offset = read_bytes;
    mapping.len = read_bytes;
    if (ioctl(snvme_helper_fd, SNVME_HELP_GET_NVME_OFFSET, &mapping) < 0) {
        perror("ioctl failed");
        assert(0);
    }
    
    nvme_ofst = mapping.address;
    close(snvme_helper_fd);
    printf("nvme_ofst is %lx,block size is %u\n",nvme_ofst,read_bytes);

    qp.cq = &ctrl->queues[0];
    qp.sq = &ctrl->queues[ctrl->cq_num];
    qp.stop = false;
    qp.num_cpls = 0;
    printf("using cq is %u, sq is %u\n",qp.cq->queue.no,qp.sq->queue.no);
    read_info.offset = nvme_ofst >> 9 ;
    read_info.num_blocks = 4096 >> 9;
    printf("offset is %lx, block num is %u\n",read_info.offset,read_info.num_blocks);
    for(int  i = 0; i < 1; i++)
        status = read_and_dump(&disk,&qp,buffer.dma,&read_info);
    
    printf("disk_read ret is %d\n",status);
    buffer2 = (int *)buffer.buffer;
    for (int i = 0; i < 256; i++) {  
        printf("%02X ", buffer2[i]); 
        if ((i + 1) % 16 == 0) {  
            printf("\n"); 
        }  
    } 
    qp.stop = true;
    close(fd);
out:
    ret = Host_file_system_exit(nvme_dev_path);
    
    if(ret < 0)
        exit(-1);
    nvm_ctrl_free(ctrl);
    exit(status);
}
