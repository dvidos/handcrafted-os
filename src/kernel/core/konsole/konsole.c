#include <klib/string.h>
#include <drivers/screen.h>
#include <cpu.h>
#include <multiboot.h>
#include <drivers/clock.h>
#include <memory/kheap.h>
#include <memory/physmem.h>
#include <konsole/readline.h>
#include <konsole/commands.h>
#include <klog.h>


extern struct command commands[];
extern int run_command(struct command *cmd, int argc, char **argv);
void execute_line(char *line);
void show_help();


/**
 * konsole is an interactive shell like thing,
 * that runs in kernel space and allows the user to play
 * (in a dangerous way) with the machine.
 */
void konsole() {
    
    readline_t *rl = init_readline("dv @ konsole $ ");

    struct command *cmd = commands;
    while (cmd->cmd != NULL) {
        readline_add_keyword(rl, cmd->cmd);
        cmd++;
    }
    
    while (true) {
        char *line = readline(rl);
        if (strcmp(line, "?") == 0 || strcmp(line, "help") == 0) {
            show_help();
        } else {
            execute_line(line);
        }
    }
}

void execute_line(char *line) {
    char tokens[80];
    char *argv[16];
    int argc;
    int (*func_ptr)();

    klog_debug("konsole executing line \"%s\"", line);

    // since strtok destroys the string, copy to temporary place
    strncpy(tokens, line, sizeof(tokens));
    char *cmd_name = strtok(tokens, " ");
    if (cmd_name == NULL || strlen(cmd_name) == 0)
        return;

    char *p;
    argc = 0;
    while (argc < 16) {
        if ((p = strtok(NULL, " ")) == NULL)
            break;
        argv[argc++] = p;
    }

    struct command *cmd = commands;
    while (cmd->cmd != NULL) {
        if (strcmp(cmd_name, cmd->cmd) == 0)
            break;
        cmd++;
    }
    if (cmd->cmd == NULL) {
        tty_write(cmd_name);
        tty_write(": command not found\n");
        return;
    }

    uint32_t initial_free = kernel_heap_free_size();
    // we shall pass the arguments, even if they go unused
    typedef int (*full_featured_func_ptr)(int argc, char **argv);
    full_featured_func_ptr ptr = (full_featured_func_ptr)cmd->func_ptr;
    int exit_code = ptr(argc, argv);

    uint32_t final_free = kernel_heap_free_size();
    if (final_free != initial_free) {
        klog_warn("Kernel heap lost %d bytes of free memory", initial_free - final_free);
    }

    kernel_heap_verify();
}

void show_help() {
    int maxlen = 0;
    struct command *cmd = commands;
    while (cmd->cmd != NULL) {
        if (strlen(cmd->cmd) > maxlen)
            maxlen = strlen(cmd->cmd);
        cmd++;
    }

    char buffer[64];
    cmd = commands;
    while (cmd->cmd != NULL) {
        int padding = maxlen + 2 - strlen(cmd->cmd);
        padding = (padding > (int)sizeof(buffer) - 1) ? (int)sizeof(buffer) - 1 : padding;
        memset(buffer, ' ', padding);
        buffer[padding] = '\0';

        tty_write(cmd->cmd);
        tty_write(buffer);
        tty_write(cmd->descr);
        tty_write("\n");
        cmd++;
    }
}


