#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// generate a init_data segment, by having some variables.
int initialized_variable_a = 1234;
int initialized_variable_b = 5678;
int initialized_variable_c = 9111;
int initialized_variable_d = 1213;
int initialized_variable_e = 1415;
int initialized_variable_f = 1617;
int initialized_variable_g = 1819;


const char some_arr[512] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g' };


int main(int argc, char *argv[]) {
    printf("Hello from init!\n");
    // int i = 1;

    write(1, "Hello stdout!\n", 14);

    initialized_variable_a += 123;
    initialized_variable_e += 666;

    printf("And here's another string, hoping for a nice rodata section! (%d) \n", 
            initialized_variable_a + initialized_variable_f);

    // int ret = fork();
    // if (ret == 0) {
    //     printf("fork returned zero\n");
    // } else {
    //     printf("fork returned %d\n", ret);
    // }

    // while (1) {
    //     printf("\n%d...", i);
    //     sleep(1000);
    //     i++;
    // }

    printf("Looping forever...");
    for(;;) {
        syslog_info("Hello from init");
        sleep(1500);
    }
}
