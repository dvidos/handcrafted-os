#ifndef _UNISTD_H
#define _UNISTD_H

#include <sys/types.h> // For pid_t, uid_t, gid_t, off_t, size_t, ssize_t

// --- Constants ---
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

// Access modes for access()
#define F_OK 0 // Test for existence
#define X_OK 1 // Test for execute permission
#define W_OK 2 // Test for write permission
#define R_OK 4 // Test for read permission

// --- Function Prototypes (based on usage analysis) ---

// File and Directory operations
int access(const char *pathname, int mode);
int chdir(const char *path);
int close(int fd);
int dup2(int oldfd, int newfd);
int link(const char *oldpath, const char *newpath);
off_t lseek(int fd, off_t offset, int whence);
ssize_t read(int fd, void *buf, size_t count);
int mkdir(const char *pathname);
int rmdir(const char *pathname);
int unlink(const char *pathname);
int symlink(const char *oldpath, const char *newpath);
int ftruncate(int fd, off_t length);
ssize_t write(int fd, const void *buf, size_t count);

// Process management
pid_t fork(void);
int execv(const char *path, char *const argv[]);
int execve(const char *path, char *const argv[], char *const envp[]);
pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);
gid_t getgid(void);
char *getcwd(char *buf, size_t size);
int setpgid(pid_t pid, pid_t pgid);
pid_t setsid(void);
void sync(void);
int pipe(int pipefd[2]);

// Terminal control
int isatty(int fd);

#endif // _UNISTD_H