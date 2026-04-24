#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <hcos/syslog.h>



int main(int argc, char *argv[]) {
    // syslog_debug("argc=%d, argv=%p", argc, argv);
    // for (int i = 0; i < argc; i++) syslog_debug("    argv[%d]=\"%s\"", i, argv[i]);
    
    for (int i = 1; i < argc; i++) {
        if (i > 1)
            printf(" ");

        printf("%s", argv[i]);
    }
    printf("\n");

    // for (int i = 0; i < 5; i++) {
    //     syslog_info("delaying %d...", i+1);
    //     sleep(1);
    // }

    return 0;
}
