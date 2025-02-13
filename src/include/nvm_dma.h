#ifndef __NVM_DMA_H__
#define __NVM_DMA_H__
// #ifndef __CUDACC__
// #define __device__
// #define __host__
// #endif

#include <nvm_types.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>




/*
 * Create DMA mapping descriptor from physical/bus addresses.
 *
 * Create a DMA mapping descriptor, describing a region of memory that is
 * accessible for the NVM controller. The caller must supply physical/bus  
 * addresses of physical memory pages, page size and total number of pages.
 * As the host's page size may differ from the controller's page size (MPS),
 * this function will calculate the necessary offsets into the actual memory
 * pages.
 *
 * While virtual memory is assumed to be continuous, the physical pages do not
 * need to be contiguous. Physical/bus addresses must be aligned to the 
 * controller's page size.
 *
 * Note: vaddr can be NULL.
 */
int nvm_dma_map(nvm_dma_t** map,                // Mapping descriptor reference
                const nvm_ctrl_t* ctrl,         // NVM controller reference
                void* vaddr,                    // Pointer to userspace memory (can be NULL if not required)
                size_t page_size,               // Physical page size
                size_t n_pages,                 // Number of pages to map
                const uint64_t* page_addrs);    // List of physical/bus addresses to the pages



/*
 * Create DMA mapping descriptor using offsets from a previously 
 * created DMA descriptor.
 */
int nvm_dma_remap(nvm_dma_t** new_map, const nvm_dma_t* other_map);



/*
 * Remove DMA mapping descriptor.
 *
 * Unmap DMA mappings (if necessary) and remove the descriptor.
 * This function destroys the descriptor.
 */
void nvm_dma_unmap(nvm_dma_t* map);

/*
 * Create DMA mapping descriptor from virtual address using the kernel module.
 * This function is similar to nvm_dma_map, except the user is not required
 * to pass physical/bus addresses. 
 *
 * Note: vaddr can not be NULL, and must be aligned to system page size.
 */
int nvm_dma_map_host(nvm_dma_t** handle, const nvm_ctrl_t* ctrl, void* vaddr, size_t size,int is_cq, int ioq_idx);


//#endif /* __CUDA__ */


#if ( !defined( __CUDA__ ) && !defined( __CUDACC__ ) ) && ( defined (__unix__) )
/* 
 * Short-hand function for allocating a page aligned buffer and mapping it 
 * for the controller.
 *
 * Note: this function will not work if you are using the CUDA API
 */
int nvm_dma_create(nvm_dma_t** map, const nvm_ctrl_t* ctrl, size_t size);
#endif


#endif /* __NVM_DMA_H__ */
