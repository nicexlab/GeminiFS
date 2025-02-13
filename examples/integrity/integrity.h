#ifndef __LIBNVM_SAMPLES_INTEGRITY_H__
#define __LIBNVM_SAMPLES_INTEGRITY_H__

#include <nvm_types.h>
#include <stdio.h>
#include <stdint.h>






int create_buffer(struct buffer* b, nvm_ctrl_t* ctrl, size_t size,int is_cq, int ioq_idx);


void remove_buffer(struct buffer* b);



int create_queue(struct queue* q, nvm_ctrl_t* ctrl, const struct queue* cq, uint16_t qno);


void remove_queue(struct queue* q);



int disk_write(const struct disk* d, struct buffer* buffer, uint16_t n_queues, off_t size,nvm_ctrl_t* ctrl);

int disk_read(const struct disk* d, struct buffer* buffer, uint16_t n_queues, off_t size,nvm_ctrl_t* ctrl);


#endif
