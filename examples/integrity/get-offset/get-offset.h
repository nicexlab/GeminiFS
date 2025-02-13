#ifndef GET_OFFSET_H
#define GET_OFFSET_H
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>  
#include <sys/ioctl.h>  
#include <stdbool.h>
struct nds_mapping {
    // input 
    int file_fd;  
	loff_t offset;  /* file offset */
	u_int64_t len;
    // output 
    u_int64_t allocated_len;
	u_int64_t address;  /* disk address */
	u_int64_t version;
	u_int8_t blkbit;
    bool exist;
};

#define SNVME_HELP_GET_NVME_OFFSET	_IOWR('N', 0x1, struct nds_mapping)

#endif