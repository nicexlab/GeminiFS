#include <nvm_ctrl.h>
#include <nvm_dma.h>
#include <nvm_error.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "integrity.h"


int create_buffer(struct buffer* b, nvm_ctrl_t* ctrl, size_t size,int is_cq, int ioq_idx)
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


void remove_buffer(struct buffer* b)
{
    nvm_dma_unmap(b->dma);
    free(b->buffer);
}


int create_queue(struct queue* q, nvm_ctrl_t* ctrl, const struct queue* cq, uint16_t qno)
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


void remove_queue(struct queue* q)
{
    remove_buffer(&q->qmem);
}

