#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// see https://github.com/mit-pdos/xv6-public/blob/master/init.c for a minimal example


/*
    TODO: for init
    - minimal FS driver for open/read/write/close for console
    - ability to register a FS driver for a prefix (e.g. "ttyX")
    - implement the dup() call through syscall
    - plain fork(), through syscall, tested and verified
    - plain exec(), after setting up i don't know what
    - push arguments and environment to stack, on proc prep
    - make a small echo program, to rpint its arguments, and test arguments
*/



// generate a init_data segment, by having some variables.
int initialized_variable_a = 1234;
int initialized_variable_b = 5678;
int initialized_variable_c = 9111;
int initialized_variable_d = 1213;
int initialized_variable_e = 1415;
int initialized_variable_f = 1617;
int initialized_variable_g = 1819;

// rodata
const char some_arr[512] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g' };


void fatal(char *msg) {
    syslog_critical(msg);
    for(;;);
}


void open_tty() {
    // opening console so all children will have these file descriptors open
    int h = open("/dev/tty0");
    if (h < 0) fatal("Failed opening tty0");

    // h == 0, stdin
    // dup(h); // 1 = stdout
    // dup(h); // 2 = stderr
}


char *load_rc_file() {
    int h = open("/etc/rc");
    if (h < 0) return NULL;

    int length = seek(h, 0, SEEK_END);

    char *buff = malloc(length + 1);
    if (buff == NULL) fatal("Could not allocate buffer for initrc");
    memset(buff, 0, length + 1);

    seek(h, 0, SEEK_SET);
    read(h, buff, length);
    close(h);
    
    return buff;
}

int execute_command(char *cmd_line) {
    // fork and exec, for testing?
    return 0;
}

int execute_rc_file() {
    char *text = load_rc_file();
    if (text == NULL) {
        printf("Error loading /etc/rc file\n");
        return -1;
    }

    int cmd_size = 4096;
    char *cmd = malloc(cmd_size);

    // now parse the good old way
    char *start = text;
    while (start != NULL && *start != 0) {
        char *end = strchr(start, '\n');
        end = end != NULL ? end : start + strlen(start);
        int cmd_len = end - start;

        cmd_len = cmd_len > cmd_size - 1 ? cmd_size - 1 : cmd_len;
        strncpy(cmd, start, cmd_len);
        cmd[cmd_len] = 0;

        start = end;
        if (*start == '\n')
            start++;

        if (strlen(cmd) == 0 || cmd[0] == '#')
            continue;

        printf("Init would execute '%s'\n", cmd);
    }

    free(text);
    free(cmd);
    return 0;
}

int main(int argc, char *argv[]) {
    printf("Hello from init!\n");
    // int i = 1;

    open_tty();
    write(0, "Hello tty!\n", 14);
    // write(1, "Hello stdout!\n", 14);
    // write(2, "Hello stderr!\n", 14);

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

    printf("Executing /etc/initrc file");
    execute_rc_file();

    printf("Looping forever...");
    for(;;) {
        syslog_info("Hello from init");
        sleep(1500);
    }
}
