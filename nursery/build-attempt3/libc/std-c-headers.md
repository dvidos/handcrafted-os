# std C headers


`sys/types.h`

- pid_t, size_t, uid_t, gid_t, off_t

`sys/wait.h`

- wait(), waitpid(), 
- WIFEXITED(status), WEXITSTATUS(status)

`signal.h`

- signal(), sigaction(), kill(), 
- SIGINT, SIGKILL, SIGCHLD

`errno.h`

- EACCESS, ENOENT, ENTRR, EAGAIN

`stdio.h`: Provides buffered input and output functions. This is the most common header in C.

- printf(), scanf(), fopen(), fread(), fwrite(), fclose(), perror()
- FILE, EOF, NULL, stdin, stdout, stderr

`stdlib.h`

- malloc(), free(), exit(), atoi() (string to int), rand(), system()
- EXIT_SUCCESS, EXIT_FAILURE

`unistd.h`: The primary header for the POSIX operating system API. This is essentially the "Linux API" header.

- read(), write(), close(), fork(), exec(), pipe(), sleep()
- STDIN_FILENO, STDOUT_FILENO

`fcntl.h`

- open(), fcntl(), creat()
- O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_APPEND

`string.h`

- strlen(), strcpy(), strcat(), strcmp(), strstr(), strtok(), memset(), memcpy()

`malloc.h`: Specialized heap functions

- mallopt() (control malloc parameters), mallinfo() (get stats)

`time.h`

- time(), ctime(), strftime(), localtime()
- struct tm (broken down time), time_t (seconds since 1970)

`ctype.h`

- isdigit(), isalpha(), isspace(), toupper(), tolower()


