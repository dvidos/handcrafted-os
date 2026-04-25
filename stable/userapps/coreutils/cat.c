#include <stdio.h>
#include <stdlib.h>

void cat_file(FILE *fp) {
    char buf[BUFSIZ];
    size_t nread;
    while ((nread = fread(buf, 1, BUFSIZ, fp)) > 0) {
        fwrite(buf, 1, nread, stdout);
    }
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        cat_file(stdin);
    } else {
        for (int i = 1; i < argc; i++) {
            FILE *fp = fopen(argv[i], "r");
            if (fp == NULL) {
                perror(argv[i]);
                return EXIT_FAILURE;
            }
            cat_file(fp);
            fclose(fp);
        }
    }
    return EXIT_SUCCESS;
}
