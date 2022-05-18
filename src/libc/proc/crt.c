#include <stddef.h>
/**
 * As at the moment I have no idea how I will exec() any user land process,
 * I am atm just interested in removing the warnings and improving the build system
 * 
 * See also https://wiki.osdev.org/Creating_a_C_Library#Program_Initialization
 */


void _start() {
    // prepare some stuff,
    // then call main()
    extern void main(int argc, char *argv[]);
    main(0, NULL);
}