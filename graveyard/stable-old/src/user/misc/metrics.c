#include <stdio.h>
#include <string.h>
#include <metrics.h>


int factorial(int n) {
    if (n == 0)
        return 1;
    
    return n * factorial(n - 1);
}

int fibonacci(int n) {
    if (n <= 1)
        return n;
    
    return fibonacci(n - 1) + fibonacci(n - 2);
}

void calculate_fibonacci_series(int numbers) {
    for (int i = 0; i < numbers; i++)
        fibonacci(i);
}


void slow_function() {
    calculate_fibonacci_series(37);
}

void medium_function() {
    calculate_fibonacci_series(30);
}

void fast_function() {
    calculate_fibonacci_series(8);
}

void very_fast_function() {
    int a = 1;
    a = a + 1;
}

void main(int argc, char *argv[]) {
    exec_metrics metrics[4];
    memset(metrics, 0, sizeof(metrics));

    // for some reason, the timer interrupt does not fire
    // when we have long computations... that's weird...
    // I think it has to do with preemptive scheduling and not having cleared the "schedule()" yet?
    __asm__("sti");

    printf("Measuring execution time... (this may take several seconds)");
    take_exec_metrics(slow_function,   "slow_function()",   &metrics[0]);
    take_exec_metrics(medium_function, "medium_function()", &metrics[1]);
    take_exec_metrics(fast_function,   "fast_function()",   &metrics[2]);
    take_exec_metrics(very_fast_function,   "very_fast()",   &metrics[3]);
    printf("\n");

    printf("Function name        iterations elapsed ms/call calls/sec trusted\n");
    //      12345678901234567890 123456789 1234567 1234567 123456789 123
    for (int i = 0; i < (int)(sizeof(metrics)/sizeof(metrics[0])); i++) {
        printf("%-20s %9u %8u %7u %9u  %s\n",
            metrics[i].name,
            metrics[i].iterations_count,
            metrics[i].iterations_msecs,
            metrics[i].msecs_per_iteration,
            metrics[i].iterations_per_second,
            metrics[i].unreliable ? "no" : "yes"
        );
    }
}
