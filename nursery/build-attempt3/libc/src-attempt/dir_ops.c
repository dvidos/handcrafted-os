#include "libc_internal.h"



DIR *opendir(const char *name) {
    // TODO: Implement this function
    return NULL;
}

struct dirent *readdir(DIR *dirp) {
    return NULL;
}

int closedir(DIR *dirp) {
    // TODO: Implement this function
    return 0;
}





// ----------------------------------- --------------------------------





int opendir(char *name) {
    return syscall(SYS_OPEN_DIR, (int)name, 0, 0, 0, 0);
}

int rewinddir(int handle) {
    return syscall(SYS_REWIND_DIR, handle, 0, 0, 0, 0);
}

dirent_t *readdir(int handle) {
    // dirent in 
    static dirent_t static_dirent;
    int err = syscall(SYS_READ_DIR, handle, (int)&static_dirent, 0, 0, 0);
    return err ? NULL : &static_dirent;
}

int closedir(int handle) {
    return syscall(SYS_CLOSE_DIR, handle, 0, 0, 0, 0);
}

int mkdir(const char *path) {
    return syscall(SYS_MKDIR, (int)path, 0, 0, 0, 0);
}

int rmdir(const char *path) {
    return syscall(SYS_RMDIR, (int)path, 0, 0, 0, 0);
}

