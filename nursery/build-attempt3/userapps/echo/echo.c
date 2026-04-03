#include <stdio.h>
#include <stdlib.h>



int main(int argc, char *argv[]) {
    // syslog_debug("argc=%d, argv=%p", argc, argv);
    // for (int i = 0; i < argc; i++) syslog_debug("    argv[%d]=\"%s\"", i, argv[i]);
    
    for (int i = 1; i < argc; i++) {
        if (i > 1)
            printf(" ");

        printf("%s", argv[i]);
    }
    printf("\n");

    return 0;
}
