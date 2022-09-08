#include <stdio.h>
#include <string.h>



int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("Ascii table presentation\n");

    char buffer[80];
    printf("   00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f \n");
    for (int i = 0; i < 16; i++) {
        sprintfn(buffer, sizeof(buffer), "%x0 ", i);

        for (int j = 0; j < 16; j++) {
            unsigned char c = (unsigned char)(i * 16 + j);
            buffer[3 + (j * 3) + 0] = c < 16 ? '.' : c;
            buffer[3 + (j * 3) + 1] = ' ';
            buffer[3 + (j * 3) + 2] = ' ';
        }
        buffer[3 + 16 * 3 + 0] = '\n';
        buffer[3 + 16 * 3 + 1] = '\0';

        printf(buffer);
    }
    
    return 0;
}


