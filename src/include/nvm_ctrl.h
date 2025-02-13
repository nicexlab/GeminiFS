#ifndef __NVM_CTRL_H__
#define __NVM_CTRL_H__
// #ifndef __CUDACC__
// #define __device__
// #define __host__
// #endif

#include <nvm_types.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>




/* 
 * Minimum size of mapped controller memory.
 */
#define NVM_CTRL_MEM_MINSIZE                        0x2000



#if defined (__unix__)
/*
 * Initialize NVM controller handle.
 *
 * Read from controller registers and initialize controller handle. 
 * This function should be used when using the kernel module or to manually
 * read from sysfs.
 *
 * Note: fd must be opened with O_RDWR and O_NONBLOCK
 */
int nvm_ctrl_init(nvm_ctrl_t** ctrl, int snvme_c_fd, int snvme_d_fd);
#endif



/* 
 * Initialize NVM controller handle.
 *
 * Read from controller registers and initialize the controller handle using
 * a memory-mapped pointer to the PCI device BAR.
 *
 * This function should be used when neither SmartIO nor the disnvme kernel
 * module are used.
 *
 * Note: ctrl_mem must be at least NVM_CTRL_MEM_MINSIZE large and mapped
 *       as IO memory. See arguments for mmap() for more info.
 */
int nvm_raw_ctrl_init(nvm_ctrl_t** ctrl);

int ioctl_set_qnum(nvm_ctrl_t* ctrl, int ioq_num);
int ioctl_use_userioq(nvm_ctrl_t* ctrl, int use);
int ioctl_reg_nvme(nvm_ctrl_t* ctrl, int reg);
int ioctl_rebind_nvme(nvm_ctrl_t* ctrl, struct pci_device_addr device_addr, int bind);
int ioctl_set_cdev(int fd_control, struct pci_device_addr device_addr, int bind);
int init_userioq(nvm_ctrl_t* ctrl, struct disk* d);
void ioctl_clear_qnum(nvm_ctrl_t* ctrl);

/*
 * Release controller handle.
 */
void nvm_ctrl_free(nvm_ctrl_t* ctrl);


struct controller* ctrl_to_controller(nvm_ctrl_t* ctrl);


#endif /* __NVM_CTRL_H__ */
