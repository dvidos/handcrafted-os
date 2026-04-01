#include <stdio.h>
#include <stdlib.h>



int main(int argc, char *argv[]) {
    // for (int i = 0; i < 5; i++) {
    //     printf("this is echo, iteration #%d, will sleep for one second\n", i);
    //     syslog_info("this is echo, iteration #%d, will sleep for one second", i);
    //     sleep(1000);
    // }

    for (int i = 1; i < argc; i++) {
        if (i > 1)
            printf(" ");

        printf("%s", argv[i]);
    }
    printf("\n");

    int exit_code = 123;
    syslog_info("this is echo, exiting with exit code %d", exit_code);
    return exit_code;
}
