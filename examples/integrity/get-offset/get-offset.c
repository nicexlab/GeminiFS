#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>  
#include <sys/ioctl.h> 
#include "get-offset.h"
#define file_path "/home/qs/fio_test/fio_test"
#define snvme_helper_path "/dev/snvme_helper"
int main(int argc, char *argv[]) 
{
    struct nds_mapping mapping;
    int fd_file, fd_dev;  
    // open file
    fd_file = open(file_path, O_RDONLY);  

    if (fd_file < 0) {  

        perror("Failed to open file");  
        return -1;  
    }
    fd_dev = open(snvme_helper_path, O_RDWR);  

    if (fd_dev < 0) {  

        perror("Failed to open fd_dev");  
        return -1;  
    } 
    mapping.file_fd = fd_file;
    mapping.offset = 0x1000;
    mapping.len = 0x1000;
    if (ioctl(fd_dev, SNVME_HELP_GET_NVME_OFFSET, &mapping) < 0) {  
        perror("ioctl failed");  
        close(fd_file);  
        close(fd_dev);  
        return -1;  
    }
    else
    {
        printf("get success");
        printf("mapping addr is %lx, allocated_len is %lx, exit is %u\n",mapping.address,mapping.allocated_len,mapping.exist);
    }
    // constrst nds mapping
    close(fd_file);  

    close(fd_dev);  
    return 0;
}
