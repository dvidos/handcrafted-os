#include <stdio.h>
#include <string.h>
#include <readline.h>

// what does a shell support?
// - environment manipulation (set, unset)
// - execute utilities
// - manipulate files (cp, mv, touch, unlink, mkdir etc)
// - manipulate devices?
// - communicate with kernel, show progress etc.
// - should be able to work with pty, in graphics mode.


readline_t *rl = NULL;

void init() {
    rl = init_readline("dv @ shell $ ");
}

void exec(char *line) {
    printf("You entered: \"%s\"\n", line);
}

int main(int argc, char *argv[], char *envp[]) {
    (void)argc;
    (void)argv;
    (void)envp;
    
    init();
    while (true) {
        char *line = readline(rl);
        if (strcmp(line, "exit") == 0)
            break;
        
        exec(line);
    }

    return 0;
}
