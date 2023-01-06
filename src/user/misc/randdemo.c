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

    if (argc > 1) {
        uint32_t seed = atoui(argv[1]);
        printf("User seed (%u): ", seed);
        srand(seed);
        for (int i = 0; i < 5; i++)
            printf("%10u, ", rand());
        printf("\n");
    } else { 
        printf("Pass a seed as first argument for testing\n");
    }
}