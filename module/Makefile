# SPDX-License-Identifier: GPL-2.0

KVER ?= $(shell uname -r)
# Try to get it based on available nvidia module version (just in case there are sources for couple of versions)
nv_version=$(shell /sbin/modinfo -F version -k $(KVER) nvidia 2>/dev/null)
nv_sources=$(shell /bin/ls -d /usr/src/nvidia-$(nv_version)/ 2>/dev/null)
ifneq ($(shell test -d "$(nv_sources)" && echo "true" || echo "" ),)
	NVIDIA_SRC_DIR ?= $(shell find "$(nv_sources)" -name "nv-p2p.h"|head -1|xargs dirname || echo "NVIDIA_DRIVER_MISSING")
else
	NVIDIA_SRC_DIR ?= $(shell find /usr/src/nvidia-* -name "nv-p2p.h"|head -1|xargs dirname || echo "NVIDIA_DRIVER_MISSING")
endif

obj-m += snvme-core.o
obj-m += snvme.o

ccflags-y += -I../src/linux
ccflags-y += -I$(NVIDIA_SRC_DIR)

ccflags-y += -I/mnt/linux/fs/ext4

snvme-core-y				:= core.o ioctl.o snvme_help.o
snvme-core-$(CONFIG_NVME_MULTIPATH)	+= multipath.o
snvme-core-$(CONFIG_BLK_DEV_ZONED)	+= zns.o
snvme-core-$(CONFIG_NVME_HWMON)		+= hwmon.o

snvme-objs := list.o ctrl.o map.o pci.o 

# KERNEL_SRC := /lib/modules/$(shell uname -r)/build
KERNEL_SRC := ../../linux

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules
clean:
	rm -fr *.o *.ko *.mod* .*.cmd .*.d Module.symvers modules.order
install:
	sudo insmod snvme-core.ko && sudo insmod snvme.ko
rm:
	sudo rmmod snvme && sudo rmmod snvme_core