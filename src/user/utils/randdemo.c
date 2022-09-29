#include <stdio.h>
#include <string.h>
#include <rand.h>


void main(int argc, char *argv[]) {

    printf("Default seed: ");
    for (int i = 0; i < 5; i++)
        printf("%10u, ", rand());
    printf("\n");

    printf("Time seed   : ");
    srand_time();
    for (int i = 0; i < 5; i++)
        printf("%10u, ", rand());
    printf("\n");

    if (argc > 0) {
        uint32_t s = atoui(argv[0]);
        printf("%u seed: ", s);
        srand(s);
        for (int i = 0; i < 5; i++)
            printf("%10u, ", rand());
        printf("\n");
    } else { 
        printf("Pass a seed as argument for testing\n");
    }
}