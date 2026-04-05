#include <sys/wait.h>
#include <stddef.h> // For size_t, NULL
#include <stdbool.h> // For bool


void abort(void) {
    // TODO: Implement this function
}

void exit(int status) {
    // TODO: Implement this function
}

char *getenv(const char *name) {
    // TODO: Implement this function
    return NULL;
}

int system(const char *command) {
    // TODO: Implement this function
    return 0;
}

pid_t fork(void) {
    // TODO: Implement this function
    return 0;
}

pid_t wait(int *stat_loc) {
    // TODO: Implement this function
    return 0;
}

pid_t waitpid(pid_t pid, int *stat_loc, int options) {
    // TODO: Implement this function
    return 0;
}

pid_t getpid(void) {
    // TODO: Implement this function
    return 0;
}

pid_t getppid(void) {
    // TODO: Implement this function
    return 0;
}

uid_t getuid(void) {
    // TODO: Implement this function
    return 0;
}

gid_t getgid(void) {
    // TODO: Implement this function
    return 0;
}

char *getcwd(char *buf, size_t size) {
    // TODO: Implement this function
    return NULL;
}

int setpgid(pid_t pid, pid_t pgid) {
    // TODO: Implement this function
    return 0;
}

pid_t setsid(void) {
    // TODO: Implement this function
    return 0;
}

int chdir(const char *path) {
    // TODO: Implement this function
    return 0;
}
