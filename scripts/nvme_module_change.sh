#!/bin/bash  
  
# 检查参数个数  
if [ "$#" -ne 1 ]; then  
    echo "Usage: $0 <origin|share|reload|reloadp>"  
    exit 1  
fi  
path="../kernel_module/nvme/host"
KDIR="/lib/modules/$(uname -r)/kernel/drivers/nvme/host"
# 定义函数来卸载和加载模块  

unload_modules() { 
    rmmod nvme 
    rmmod nvme-core 
}  

unload_modified_modules() {
    rmmod snvme  
    rmmod snvme-core
} 

unload_modified_modules_pci_only() {
    rmmod snvme  
}  

load_custom_modules() {
    local path=$1    
    insmod $path/nvme-core.ko  
    insmod $path/nvme.ko  
}  
load_modified_modules() {  
    local path=$1  
    insmod $path/snvme-core.ko  
    insmod $path/snvme.ko  
}  

load_modified_modules_pci_only() {  
    local path=$1  
    insmod $path/snvme.ko  
}  
# 根据参数决定操作  
case "$1" in  
    "share")  
        unload_modules  
        load_modified_modules $path 
        ;;  
    "origin")  
        unload_modified_modules  
        # 注意：这里假设了内核版本为5.15，根据实际情况可能需要调整  
        load_custom_modules $KDIR  
        ;;
    "reload")
        unload_modified_modules
        load_modified_modules $path
    ;; 
    "reloadp")
        unload_modified_modules_pci_only
        load_modified_modules_pci_only $path
    ;;  
    *)  
        echo "Invalid parameter. Usage: $0 <origin|share|reload|reloadp>"  
        exit 1  
        ;;  
esac