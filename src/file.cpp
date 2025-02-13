#include <iostream>  
#include <cstdlib>  
#include <cstring>  
#include <unistd.h>

void executeCommand(const char *command) {  
    std::cerr << "Executing: " << command << std::endl;  
    if (std::system(command) != 0) {  
        std::cerr << "Failed to execute command: " << command << std::endl;  
        exit(EXIT_FAILURE);  
    }  
}  
  
bool checkFileSystem(const char *device) {  
    char command[256];  
    snprintf(command, sizeof(command), "blkid %s | grep -q ext4", device);  
    return std::system(command) == 0;  
}  
  
void createFileSystem(const char *device) {  
    char command[256];  
    snprintf(command, sizeof(command), "mkfs.ext4 %s", device);  
    executeCommand(command);  
}  

void syncFileSystem(const char *device) {  
    char command[256];  
    snprintf(command, sizeof(command), "sync %s", device);  
    executeCommand(command);  
}  

bool mountDevice(const char *device, const char *mountPoint) {  
    char command[256];  
    snprintf(command, sizeof(command), "mount %s %s", device, mountPoint);  
    if (std::system(command) != 0) {  
        std::cerr << "Failed to mount device. Attempting to recreate filesystem..." << std::endl;  
        createFileSystem(device);  
        // Try mounting again after creating the filesystem  
        if (std::system(command) != 0) {  
            std::cerr << "Failed to mount device even after recreating filesystem." << std::endl;  
            return false;  
        }  
    }  
    return true;  
}  
  
bool umountDevice(const char *device) {  
    char command[256];  
    snprintf(command, sizeof(command), "umount %s", device);  
    return std::system(command) == 0;  
}
bool lumountDevice(const char *device) {  
    char command[256];  
    snprintf(command, sizeof(command), "umount %s", device);  
    return std::system(command) == 0;  
}

int Host_file_system_int(const char *device, const char *mountPoint)
{
    if (!checkFileSystem(device)) {  
        std::cerr << "Filesystem not detected. Creating ext4 filesystem..." << std::endl;  
        createFileSystem(device);  
    } 
    if (!mountDevice(device, mountPoint)) {  
        std::cerr << "Unable to mount the device. Exiting..." << std::endl;  
        return EXIT_FAILURE;  
    }  
    sleep(2);
    std::cerr << "Device mounted successfully at " << mountPoint << std::endl;  
}

int Host_file_system_exit(const char *mountPoint)
{
    int ret = -1;
    syncFileSystem(mountPoint);
    for (int i = 0; i < 3; ++i) {  
        if (umountDevice(mountPoint)) {  
            std::cerr << "Device umounted successfully." << std::endl;  
            ret = 0;
            break;  
            
        } else {  
            syncFileSystem(mountPoint);
            std::cerr << "Failed to umount the device. Attempt " << (i + 1) << ". Retrying..." << std::endl;  

            sleep(2);
        }  
    } 
    return ret;
}

  
