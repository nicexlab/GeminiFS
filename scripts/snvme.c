#include "linux/ioctl.h"
#include <stdio.h>  
#include <stdlib.h>  
#include <fcntl.h>  
#include <sys/ioctl.h>  
#include <unistd.h>  
//  gcc -o snvme snvme.c

struct dev_addr{
    int bus;
    int slot;
    int func;
};

enum snvm_ctrl_ioctl_type{
    SNVM_REGISTER_DRIVER    = _IO('N', 0x0),
    SNVM_UNREGISTER_DRIVER  = _IO('N', 0x1),
    SNVM_DEVICE_BIND        = _IOW('N', 0x2, struct pci_device_addr),
    SNVM_DEVICE_UNBIND      = _IOW('N', 0x3, struct devpci_device_addr_addr),
    SNVM_SET_CDEV           = _IOW('N', 0x4, struct pci_device_addr),
    SNVM_CLEAR_CDEV         = _IOW('N', 0x5, struct pci_device_addr)
};

int main(int argc, char *argv[]) {  
    if (argc != 3) {  
        fprintf(stderr, "Usage: %s <device> <command>\n", argv[0]);  
        fprintf(stderr, "command: 1 for ioctl command 1, 0 for ioctl command 0\n");  
        return 1;  
    }  
  
    const char *device = argv[1];  
    int command = atoi(argv[2]);  
  
    if (command != 0 && command != 1 && command != 2 && command != 3) {  
        fprintf(stderr, "Invalid command. Command should be either 0 or 1 or 2 or 3.\n");  
        return 1;  
    }  
  
    int fd = open(device, O_RDWR);  
    if (fd == -1) {  
        perror("Failed to open device");  
        return 1;  
    }  
    int ioctl_command;
    switch(command){
        case 0:
            ioctl_command = SNVM_DEVICE_UNBIND;
            break;
        case 1:
            ioctl_command = SNVM_DEVICE_BIND;
            break;
        case 2:
            ioctl_command = SNVM_SET_CDEV;
            break;
        case 3:
            ioctl_command = SNVM_CLEAR_CDEV;
            break;
        default:
            break;
    }
    struct pci_device_addr bind_device = {
        .bus = 0xc3,
        .slot = 0,
        .func = 0,
    };

    int result = ioctl(fd, ioctl_command, &bind_device);  
    if (result == -1) {  
        perror("Failed to execute ioctl command");  
        close(fd);  
        return 1;  
    }  
  
    close(fd);  
    return 0;  
}