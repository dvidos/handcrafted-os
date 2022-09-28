#include <stdio.h>
#include <stdlib.h>


int main(int argc, char *argv[], char **envp) {
    printf("Environment\n");
    char **p = envp;
    while (*p != NULL) {
        printf("  %s\n", *p);
        p++;
    }

    printf("Arguments\n");
    for (int i = 0; i < argc; i++) {
        printf("  argv[%d] = \"%s\"\n", i, argv[i]);
    }

    printf("\n");

    return 0;
}


