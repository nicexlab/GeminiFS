#include <cassert>
#include <iostream>
#include <ctime>
#include "linux/ioctl.h"
#include "geminifs_api.h"

struct GpuTimer
{
      cudaEvent_t start;
      cudaEvent_t stop;

      GpuTimer() {
            cudaEventCreate(&start);
            cudaEventCreate(&stop);
      }

      ~GpuTimer() {
            cudaEventDestroy(start);
            cudaEventDestroy(stop);
      }

      void Start() {
            cudaEventRecord(start, 0);
      }

      void Stop() {
            cudaEventRecord(stop, 0);
      }

      float Elapsed() {
            float elapsed;
            cudaEventSynchronize(stop);
            cudaEventElapsedTime(&elapsed, start, stop);
            return elapsed;
      }
};

#define gpuErrchk(ans) { gpuAssert((ans), __FILE__, __LINE__); }
inline void gpuAssert(cudaError_t code, const char *file, int line, bool abort=true) {
   if (code != cudaSuccess) {
      fprintf(stderr,"GPUassert: %s %s %d\n", cudaGetErrorString(code), file, line);
      if (abort) exit(code);
   }
}

#define my_assert(code) do { \
    if (!(code)) { \
        host_close_all(); \
        exit(1); \
    } \
} while(0)


#define snvme_control_path "/dev/snvm_control"
#define snvme_path "/dev/csnvme1"
#define nvme_dev_path "/dev/snvme0n1"
#define snvme_helper_path "/dev/snvme_helper"
#define nvme_mount_path "/mnt/nvm_mount"
#define nvme_pci_addr {0xc3, 0, 0}

#define geminifs_file_name "checkpoint.geminifs"
#define geminifs_file_path (nvme_mount_path "/" geminifs_file_name)

#define NR_WARPS 1
#define NR_PAGES__PER_WARP 1

int
main() {
    host_open_all(
            snvme_control_path,
            snvme_path,
            nvme_dev_path,
            nvme_mount_path,
            nvme_pci_addr,
            1,
            1024,
            64);

    int nr_warps = NR_WARPS;

  size_t file_block_size = 4 * (1ull << 10);
  size_t dev_page_size = 128 * (1ull << 10);


  size_t nr_pages = nr_warps * NR_PAGES__PER_WARP;
  size_t page_capacity = nr_pages * dev_page_size;
  size_t virtual_space_size = page_capacity * 8;

  srand(time(0));
  int rand_start = rand();

  remove(geminifs_file_path);

  host_fd_t host_fd = host_create_geminifs_file(geminifs_file_path, file_block_size, virtual_space_size);
  host_refine_nvmeofst(host_fd);

  uint64_t *buf1 = (uint64_t *)malloc(virtual_space_size);
  for (size_t i = 0; i < virtual_space_size / sizeof(uint64_t); i++)
      buf1[i] = rand_start + i;
  host_xfer_geminifs_file(host_fd, 0, buf1, virtual_space_size, 0);
  
  uint64_t *buf2 = (uint64_t *)malloc(virtual_space_size);
  // todo 
  host_xfer_geminifs_file(host_fd, 0, buf2, virtual_space_size, 1);
  for (size_t i = 0; i < virtual_space_size / sizeof(uint64_t); i++)
      my_assert(buf2[i] == rand_start + i);

  host_close_geminifs_file(host_fd);

  printf("ALL OK!\n");
  host_close_all();

  return 0;
}

