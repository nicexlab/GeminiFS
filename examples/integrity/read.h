#ifndef __LIBNVM_SAMPLES_READ_BLOCKS_READ_H__
#define __LIBNVM_SAMPLES_READ_BLOCKS_READ_H__

#include <stdint.h>
#include <stdbool.h>
#include <nvm_types.h>


struct file_info
{
    size_t      queue_size;
    size_t      chunk_size;
    uint32_t    namespace_id;
    size_t      num_blocks;
    size_t      offset;
    FILE*       output;
    FILE*       input;
    bool        ascii;
    bool        identify;
};

/*
 * Information about controller and namespace.
 */



struct queue_pair
{
    struct queue* sq;
    struct queue* cq;
    bool        stop;
    size_t      num_cpls;
};





int read_and_dump(const struct disk* disk, struct queue_pair* qp, const nvm_dma_t* buffer, const struct file_info* args);




#endif
