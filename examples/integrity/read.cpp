#include "read.h"
#include <nvm_types.h>
#include <nvm_util.h>
#include <nvm_queue.h>
#include <nvm_cmd.h>
#include <nvm_error.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define MIN(a, b) ((a) <= (b) ? (a) : (b))

uint64_t timediff_us(struct timespec* start, struct timespec* end) {
    return (uint64_t)(end->tv_sec - start->tv_sec) * 1000000 + (end->tv_nsec-start->tv_nsec) / 1000;
}

void print_stats(struct timespec* start, struct timespec* end, size_t bytes) {
        uint64_t diff = timediff_us(start, end);
        fprintf(stderr, "Done in %lldus, %fMB/s\n", (unsigned long long) diff, (double)bytes/(double)diff);
}

static void* consume_completions(struct queue_pair* qp)
{
    nvm_cpl_t* cpl;

    qp->stop = false;
    qp->num_cpls = 0;
    nvm_queue_t* cq;
    nvm_queue_t* sq;
    cq = &qp->cq->queue;
    sq = &qp->sq->queue;
    while (!qp->stop)
    {
        if ((cpl = nvm_cq_dequeue_block(cq, 100)) == NULL)
        {
            usleep(1);
            continue;
        }
        nvm_sq_update(sq);
        if (!NVM_ERR_OK(cpl))
        {
            fprintf(stderr, "%s\n", nvm_strerror(NVM_ERR_STATUS(cpl)));
        }
        usleep(1);
        nvm_cq_update(cq);
        qp->num_cpls++;
    }

    return NULL;
}


static size_t rw_bytes(const struct disk* disk, struct queue_pair* qp, const nvm_dma_t* buffer, uint64_t* blk_offset, size_t* size_remaining, uint8_t op)
{
    // Read blocks
    size_t page = 0;
    size_t num_cmds = 0;
    size_t num_pages = disk->max_data_size / disk->page_size;
    size_t chunk_pages = MIN(buffer->n_ioaddrs, NVM_PAGE_ALIGN(*size_remaining, disk->page_size) / disk->page_size);
    size_t offset = *blk_offset;
    nvm_prp_list_t list;
    nvm_queue_t* sq;
    sq = &qp->sq->queue;
    
    while (page < chunk_pages)
    {
        num_pages = MIN(buffer->n_ioaddrs - page, num_pages);

        nvm_cmd_t* cmd;
        while ((cmd = nvm_sq_enqueue(sq)) == NULL)
        {
            printf("rw_bytes submit 0");
            nvm_sq_submit(sq);
            usleep(1);
        }

        uint16_t prp_list = num_cmds % sq->qs;
        size_t num_blocks = NVM_PAGE_TO_BLOCK(disk->page_size, disk->block_size, num_pages);
        size_t start_block = offset + NVM_PAGE_TO_BLOCK(disk->page_size, disk->block_size, page);
        nvm_cmd_header(cmd, NVM_DEFAULT_CID(sq), op, disk->ns_id);
        list = NVM_PRP_LIST(qp->sq->qmem.dma, NVM_SQ_PAGES(qp->sq->qmem.dma, sq->qs) + prp_list);
        page += nvm_cmd_data(cmd, 1, &list, num_pages, &buffer->ioaddrs[page]);
        nvm_cmd_rw_blks(cmd, start_block, num_blocks);
        ++num_cmds;
        usleep(1);
    }
   
    nvm_sq_submit(sq);
    *blk_offset = offset + NVM_PAGE_TO_BLOCK(disk->page_size, disk->block_size, page);
    *size_remaining -= MIN(*size_remaining, chunk_pages * disk->page_size);
    return num_cmds;
}

int read_and_dump(const struct disk* disk, struct queue_pair* qp, const nvm_dma_t* buffer, const struct file_info* args)
{
    int status;
    pthread_t completer;
    struct timespec start, end;

    // Start consuming
    status = pthread_create(&completer, NULL, (void *(*)(void*)) consume_completions, qp);
    if (status != 0)
    {
        fprintf(stderr, "Could not start completer thread\n");
        return status;
    }
    
    // Clear all PRP lists
    size_t sq_pages = NVM_SQ_PAGES(qp->sq->qmem.dma, qp->sq->queue.qs);
    memset(NVM_DMA_OFFSET(qp->sq->qmem.dma, sq_pages), 0, qp->sq->qmem.dma->page_size * (qp->sq->qmem.dma->n_ioaddrs - sq_pages));

    size_t num_cmds = 0;
    uint64_t start_block = args->offset ;
    size_t size_remaining = args->num_blocks * disk->block_size;
    while (size_remaining != 0)
    {
        fprintf(stderr, "Reading %zu bytes [%zu MB] (total=%zu)\n", 
                buffer->n_ioaddrs * disk->page_size, 
                (buffer->n_ioaddrs * disk->page_size) >> 20,
                args->num_blocks * disk->block_size - size_remaining);
        size_t remaining = size_remaining;

        clock_gettime(CLOCK_MONOTONIC, &start);

        num_cmds += rw_bytes(disk, qp, buffer, &start_block, &size_remaining, NVM_IO_READ);
        while (qp->num_cpls < num_cmds)
        {
            usleep(1);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);

        print_stats(&start, &end, remaining - size_remaining);

    }

    // Wait for completions
    qp->stop = true;
    pthread_join(completer, NULL);

    return 0;
}



