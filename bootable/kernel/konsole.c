#include <stdbool.h>
#include <stdint.h>
#include "string.h"
#include "screen.h"
#include "keyboard.h"
#include "ports.h"

/**
 * konsole is an interactive shell like thing,
 * that runs in kernel space and allows the user to play
 * (in a dangerous way) with the machine.
 */

char command[1024] = {0,};
char prev_command[1024] = {0,};
int command_len = 0;

typedef int (*runnable_command_ptr)(int argc, char *argv[]);

struct action {
    char *cmd;
    char *descr;
    runnable_command_ptr func_ptr;
};

static int do_help(int argc, char *argv[]);
static int do_say(int argc, char *argv[]);
static int do_inb(int argc, char *argv[]);
static int do_outb(int argc, char *argv[]);
static int do_inw(int argc, char *argv[]);
static int do_outw(int argc, char *argv[]);
static int do_mem_dump(int argc, char *argv[]);
static void get_command(char *prompt);
static void run_command();

struct action actions[] = {
    {"?", "Show help", do_help},
    {"help", "Show help", do_help},
    {"say", "Print arguments (to test parsing)", do_say},
    {"inb", "Read byte from port - syntax: inb port", do_inb},
    {"outb", "Write byte to port - syntax: outb port byte", do_outb},
    {"inw", "Read word from port - syntax: inw port", do_inw},
    {"outw", "Write word to port - syntax: outw port word", do_outw},
    // {"mpeek", "Read byte at physical memory - syntax: mpeek address", do_mem_peek},
    // {"mpoke", "Write byte at physical memory - syntax: mpoke address byte", do_mem_poke},
    // {"mpeekw", "Read word at physical memory - syntax: mpeekw address", do_mem_peek},
    // {"mpokew", "Write word at physical memory - syntax: mpokew address word", do_mem_poke},
    {"mdump", "Do hex memory dump - syntax: mdump address length", do_mem_dump},
    // {"hdump", "Do memory heap dump, malloc blocks", do_heap_dump},
    // {"bootinfo", "Show information from multiboot spec", do_boot_info},
    // {"pdump", "Show information about OS processes", do_proc_dump},
};


void konsole() {
    while (true) {
        get_command("> ");
        run_command();
    }
}

static int do_help(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    // print list of commands and maybe brief usage
    printf("Execute arbitrary commands and functions in kernel space\n");
    printf("Each command has one or more arguments\n");
    printf("Numbers can be decimal, octal if prefixed with '0', hex if prefixed with '0x' or binary if prefixed with 'b'\n");
    printf("\n");

    int len = sizeof(actions) / sizeof(struct action);
    for (int i = 0; i < len; i++) {
        if (strcmp(actions[i].cmd, "help") == 0 || strcmp(actions[i].cmd, "?") == 0)
            continue;
        printf("%s\n\t%s\n", actions[i].cmd, actions[i].descr);
    }

    return 0;
}

static int do_inb(int argc, char *argv[]) {
    if (argc < 1) {
        printf("Expecting port as first argument");
        return 1;
    }
    int port = atoi(argv[0]);
    uint8_t value = port_byte_in(port);
    printf("0x%x", value);
    return 0;
}

static int do_outb(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Expecting port as first argument, value as the second\n");
        return 1;
    }
    int port = atoi(argv[0]);
    uint8_t value = atoi(argv[1]) & 0xFF;
    port_byte_out(port, value);
    return 0;
}

static int do_inw(int argc, char *argv[]) {
    if (argc < 1) {
        printf("Expecting port as first argument");
        return 1;
    }
    int port = atoi(argv[0]);
    uint16_t value = port_word_in(port);
    printf("0x%x", value);
    return 0;
}

static int do_outw(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Expecting port as first argument, value as the second\n");
        return 1;
    }
    int port = atoi(argv[0]);
    uint16_t value = atoi(argv[1]) & 0xFFFF;
    port_byte_out(port, value);
    return 0;
}

static int do_say(int argc, char *argv[]) {
    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = \"%s\"\n", i, argv[i]);
    }
    for (int i = 0; i < argc; i++) {
        int parsed = atoi(argv[i]);
        printf("atoi(\"%s\") --> %d (%bb, 0%o, 0x%x)\n", argv[i], parsed, parsed, parsed, parsed);
    }
    return 0;
}

static char printable(char c) {
    return (c >= ' ' && 'c' <= '~' ? c : '.');
}

static int do_mem_dump(int argc, char *argv[]) {
    if (argc > 2) {
        printf("Expecting address as first argument, length (bytes) as the second\n");
        printf("Address defaults to succeeding previously viewed\n");
        printf("Length defaults to 256 bytes");
        return 1;
    }

    static uint32_t next_address = 0;
    uint32_t address = argc > 0 ? (uint32_t)atoi(argv[0]) : next_address;
    int len = argc == 2 ? atoi(argv[1]) : 256;
    unsigned char *ptr = (unsigned char *)address;
    while (len > 0) {
        // using xxd's format, seems nice
        printf("%08p: %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x  %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
            ptr,
            ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7],
            ptr[8], ptr[9], ptr[10], ptr[11], ptr[12], ptr[13], ptr[14], ptr[15],
            printable(ptr[0]), printable(ptr[1]), printable(ptr[2]), printable(ptr[3]),
            printable(ptr[4]), printable(ptr[5]), printable(ptr[6]), printable(ptr[7]),
            printable(ptr[8]), printable(ptr[9]), printable(ptr[10]), printable(ptr[11]),
            printable(ptr[12]), printable(ptr[13]), printable(ptr[14]), printable(ptr[15])
        );
        ptr += 16;
        len -= 16;
    }

    next_address = (uint32_t)ptr;
    return 0;
}

static void get_command(char *prompt) {
    // honor backspace, Ctrl+U (delete all), Ctrl+L (clear),
    // maybe do history with arrow keys.
    // maybe do auto completion with tab.
    struct key_event event;

    // collect key presseses, until enter is pressed
    memset(command, 0, sizeof(command));
    command_len = 0;
    printf("%s", prompt);
    while (true) {
        wait_keyboard_event(&event);
        if (event.special_key == KEY_ENTER) {
            strcpy(prev_command, command);
            printf("\r\n");
            return;
        } else if (event.special_key == KEY_UP && strlen(prev_command) > 0) {
            strcpy(command, prev_command);
            command_len = strlen(command);
            printf("\r%79s\r%s%s", " ", prompt, command);
        } else if (event.special_key == KEY_ESCAPE) {
            command_len = 0;
            command[command_len] = '\0';
            printf(" (esc)\n%s", prompt);
        } else if (event.special_key == KEY_BACKSPACE) {
            if (command_len > 0) {
                command[--command_len] = '\0';
                printf("\b \b");
            }
        } else if (event.ctrl_down) {
            switch (event.printable) {
                case 'l':
                    screen_clear();
                    printf("%s%s", prompt, command);
                    break;
                case 'u':
                    command_len = 0;
                    command[command_len] = '\0';
                    printf("\r%79s\r%s%s", " ", prompt, command);
                    break;
            }
        } else if (event.printable) {
            command[command_len++] = event.printable;
            command[command_len] = '\0';
            printf("%c", event.printable);
        }
    }
}

static void run_command() {
    // parse the command and arguments
    char *cmd_name = strtok(command, " ");
    if (cmd_name == NULL || strlen(cmd_name) == 0)
        return;

    char *argv[16];
    int argc = 0;
    memset((char *)argv, 0, sizeof(argv));
    while (argc < 16) {
        char *p = strtok(NULL, " ");
        if (p == NULL)
            break;
        if (strlen(p) > 0) {
            argv[argc++] = p;
        }
    }

    int num_actions = sizeof(actions) / sizeof(actions[0]);
    int target = -1;
    for (int i = 0; i < num_actions; i++) {
        if (strcmp(cmd_name, actions[i].cmd) == 0) {
            target = i;
            break;
        }
    }

    if (target == -1) {
        printf("Unknown command \"%s\"\n", cmd_name);
        return;
    }

    int return_value = actions[target].func_ptr(argc, argv);
    if (return_value != 0) {
        printf("\nProcess exited with exit value %d", return_value);
    }
}
