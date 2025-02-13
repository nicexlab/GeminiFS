#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <cuda_runtime.h>

#include "geminifs_api.h"
#include "get-offset.h"

#define my_assert(code) do { \
    if (!(code)) { \
        assert(0); \
    } \
} while(0)


union geminiFS_magic
the_geminiFS_magic = {
  .magic_cstr = {'g', 'e', 'm', 'i', 'n', 'i', 'f', 's'}
};

static int
one_nr__of__binary_int(unsigned long long i) {
  int count = 0;
  while (i != 0) {
    if ((i & 1) == 1)
      count++;
    i = i >> 1;
  }
  return count;
}

static rawfile_ofst_t
host__convert_va__to(host_fd_t host_fd, vaddr_t va) {
  struct geminiFS_hdr *hdr = host_fd;
  return hdr->first_block_base + va;
}

static void
host_for_each_table_entry(host_fd_t fd,
                          nvme_ofst_t fun(vaddr_t va,
                                          nvme_ofst_t orig_nvmeofst,
                                          void *context),
                          void *context) {
  struct geminiFS_hdr *hdr = fd;
  struct geminiFS_hdr *file_mmap = (struct geminiFS_hdr *)mmap(NULL,
                                                              hdr->first_block_base,
                                                              PROT_WRITE | PROT_READ,
                                                              MAP_SHARED,
                                                              fd->fd,
                                                              0);
  my_assert((void *) -1 != file_mmap);
  for (size_t i = 0; i < hdr->nr_l1; i++) {
    nvme_ofst_t new_nvmeofst = fun(i << hdr->block_bit, file_mmap->l1[i], context);
    if (new_nvmeofst != 0xffffffff)
      file_mmap->l1[i] = new_nvmeofst;
  }
  munmap(file_mmap, hdr->first_block_base);
}

#define ROUND_UP(x, align)(((uint64_t) (x) + ((uint64_t)align - 1)) & ~((uint64_t)align - 1))
host_fd_t
host_create_geminifs_file_1(const char *filename,
                          uint64_t block_size,
			  uint64_t page_size,
                          uint64_t virtual_space_size) {
	return host_create_geminifs_file(filename, block_size, ROUND_UP(virtual_space_size, page_size));
}


host_fd_t
host_create_geminifs_file(const char *filename,
                          uint64_t block_size,
                          uint64_t virtual_space_size) {
  struct geminiFS_hdr *hdr;
  fd_t fd;

  my_assert(virtual_space_size % block_size == 0);

  hdr = (struct geminiFS_hdr*)malloc(sizeof(struct geminiFS_hdr));
  hdr->magic_num = the_geminiFS_magic.magic_num;
  hdr->virtual_space_size = ROUND_UP(virtual_space_size, block_size);
  hdr->block_bit = one_nr__of__binary_int(block_size - 1);
  hdr->nr_l1 = hdr->virtual_space_size >> hdr->block_bit;
  hdr->first_block_base = ROUND_UP(sizeof(struct geminiFS_hdr) + sizeof(nvme_ofst_t) * hdr->nr_l1, block_size);

  fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  my_assert(0 <= fd);

  my_assert(0 ==
    ftruncate(fd, hdr->first_block_base + virtual_space_size));

  my_assert((off_t)(-1) != lseek(fd, 0, SEEK_SET));
  my_assert(sizeof(*hdr) == write(fd, hdr, sizeof(*hdr)));

  hdr->fd = fd;

  void *t = malloc(virtual_space_size);
  host_xfer_geminifs_file(hdr, 0, t, virtual_space_size, 0);

  return hdr;
}

host_fd_t
host_open_geminifs_file(const char *filename) {
  struct geminiFS_hdr *hdr = (struct geminiFS_hdr*)malloc(sizeof(struct geminiFS_hdr));
  fd_t fd = open(filename, O_RDWR);
  my_assert(0 <= fd);

  my_assert((off_t)(-1) != lseek(fd, 0, SEEK_SET));
  my_assert(sizeof(*hdr) == read(fd, hdr, sizeof(*hdr)));

  hdr->fd = fd;

  my_assert(hdr->magic_num == the_geminiFS_magic.magic_num);

  return hdr;
}

size_t
host_xfer_geminifs_file(host_fd_t host_fd,
                        vaddr_t va,
                        void *buf_1,
                        size_t nbyte,
                        int is_read) {
  struct geminiFS_hdr *hdr = host_fd;
  fd_t fd = hdr->fd;
  my_assert((off_t)(-1) != lseek(fd, host__convert_va__to(host_fd, va), SEEK_SET));

  size_t nbyte_already = 0;
  char *buf = (char *)buf_1;
  while (0 < nbyte) {
    size_t nbyte_this_time;
    if (is_read)
      nbyte_this_time = read(fd, buf, nbyte);
    else
      nbyte_this_time = write(fd, buf, nbyte);
    my_assert(nbyte_this_time != -1);

    nbyte -= nbyte_this_time;
    buf += nbyte_this_time;
    nbyte_already += nbyte_this_time;
  }
  if (!is_read)
      fsync(fd);
  return nbyte_already;
}

void
host_close_geminifs_file(host_fd_t fd) {
  close(fd->fd);
  free(fd);
}

struct refine_nvmeofst_context {
    host_fd_t host_fd;
    int fd_file;
    int snvme_helper_fd;
    int block_size;
};
static nvme_ofst_t
host_refine_nvmeofst_1(vaddr_t va,
                       nvme_ofst_t orig_nvmeofst,
                       void *context_1) {
  struct refine_nvmeofst_context *context = (struct refine_nvmeofst_context *)context_1;
  struct nds_mapping mapping;
  mapping.file_fd = context->fd_file;
  mapping.offset = host__convert_va__to(context->host_fd, va);
  mapping.len = context->block_size;
  if (ioctl(context->snvme_helper_fd, SNVME_HELP_GET_NVME_OFFSET, &mapping) < 0) {
      perror("ioctl failed");
      my_assert(0);
  } 
  return mapping.address;
}

#define snvme_helper_path "/dev/snvme_helper"
void
host_refine_nvmeofst(host_fd_t fd) {
  int snvme_helper_fd = open(snvme_helper_path, O_RDWR);
  if (snvme_helper_fd < 0) {
      perror("Failed to open snvme_helper_fd");
      my_assert(0);
  }
  struct refine_nvmeofst_context c;
  c.host_fd = fd;
  c.fd_file = fd->fd;
  c.snvme_helper_fd = snvme_helper_fd;
  c.block_size = 1 << fd->block_bit;
  host_for_each_table_entry(fd, host_refine_nvmeofst_1, &c);
  close(snvme_helper_fd);
}

static size_t
raw_file_size(int fd) {
  return lseek(fd, 0, SEEK_END);
}


