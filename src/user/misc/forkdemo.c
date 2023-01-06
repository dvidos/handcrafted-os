#include <stdio.h>
#include <stdlib.h>



int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("Hi, I am the parent, PID %d, PPID %d\n", getpid(), getppid());
    int err = fork();
    if (err < 0) {
        printf("This is the parent, fork() failed, bummer, err = %d\n", err);
        return 1;

    } else if (err == 0) {
        printf("This is the child, my PID is %d, my PPID is %d\n", getpid(), getppid());
        sleep(2000);
        printf("This is the child, I'll exit now, returning 15\n");
        return 15;

    } else if (err > 0) {
        printf("This is the father, child pid is %d, I'll wait for it\n", err);
        int exit_code = 0;
        int err = wait(&exit_code);
        if (err < 0) {
            printf("This is the father, wait() failed, err = %d\n", err);
        } else {
            printf("This is the father, child pid %d exited with exit code %d\n", err, exit_code);
        }
    }

    return 0;
}


