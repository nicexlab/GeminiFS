#!/bin/bash  
  
# Check if nvme and snvme devices exist  
if ls /dev/ | grep -q "nvme0n1" && ls /dev/ | grep -q "snvme"; then  
    echo "Both nvme and snvme devices detected. Running unreg_nvme script..."  
    ./nvme_unreg /dev/snvm_control 0
    echo "Running nvme_module_change.sh with reload option..."  
    ./nvme_module_change.sh reload  
elif ls /dev/ | grep -q "nvme0n1" && ! ls /dev/ | grep -q "snvme"; then  
    echo "Detected nvme device, but no snvme device detected."  
      
    # Get a list of nvme devices  
    nvme_devices=$(ls /dev/nvme*n1)  
      
    for device in $nvme_devices; do  
        # Check if the device is mounted  
        mount | grep -q "$device"  
        if [ $? -eq 0 ]; then  
            echo "$device is mounted, unmounting..."  
            umount "$device"  
            if [ $? -eq 0 ]; then  
                echo "Unmount successful."  
            else  
                echo "Unmount failed, exiting script."  
                exit 1  
            fi  
        fi  
    done  
      
    # Run the snvme_share.sh script  
    echo "Running snvme_share.sh..."  
    ./nvme_module_change.sh share  
else  
    echo "No nvme device detected, but snvme device detected. Running nvme_module_change.sh with reload option..."  
    ./nvme_module_change.sh reload  
fi