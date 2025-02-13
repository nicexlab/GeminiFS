#ifndef __NVM_INTERNAL_FILE_H__
#define __NVM_INTERNAL_FILE_H__


int Host_file_system_int(const char *device, const char *mountPoint);

int Host_file_system_exit(const char *mountPoint);

#endif 