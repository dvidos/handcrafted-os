#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h> // For bool type

// Definitions for common types
typedef int pid_t;
typedef long ssize_t;
typedef long off_t;

// Forward declaration for fatal, as it uses syslog_critical
void fatal(char *msg);


#define MAX_ARGS 16
#define MAX_ARG_LEN 256
#define MAX_PATH_LEN 256


typedef enum {
    CMD_TYPE_ONCE,
    CMD_TYPE_REPEAT
} cmd_type_t;

typedef struct {
    cmd_type_t type;
    char path[MAX_PATH_LEN];
    char *argv[MAX_ARGS + 1]; // +1 for NULL terminator
    int argc;
    pid_t pid; // Process ID if spawned
    bool active; // Is this command currently running or waiting to be respawned
    bool spawned_once; // Flag to track if 'once' commands have been spawned
} cmd_entry_t;

typedef struct cmd_list_node {
    cmd_entry_t *command;
    struct cmd_list_node *next;
} cmd_list_node_t;

typedef struct {
    cmd_list_node_t *head;
    int count;
} cmd_list_t;

// Function to free resources associated with a cmd_entry_t object
void cmd_destroy(cmd_entry_t *cmd) {
    if (!cmd) return;
    for (int i = 0; i < cmd->argc; i++) {
        free(cmd->argv[i]);
    }
    free(cmd);
}

// Initialize command list
void cmd_list_init(cmd_list_t *list) {
    list->head = NULL;
    list->count = 0;
}

// Add a command to the list (takes ownership of cmd pointer)
void cmd_list_add(cmd_list_t *list, cmd_entry_t *cmd) {
    cmd_list_node_t *new_node = (cmd_list_node_t *)malloc(sizeof(cmd_list_node_t));
    if (!new_node) syslog_critical("Failed to allocate cmd_list_node_t");
    new_node->command = cmd;
    new_node->next = list->head;
    list->head = new_node;
    list->count++;
}

// Destroy command list and its contents
void cmd_list_destroy(cmd_list_t *list) {
    cmd_list_node_t *current = list->head;
    while (current) {
        cmd_list_node_t *next = current->next;
        cmd_destroy(current->command);
        free(current);
        current = next;
    }
    list->head = NULL;
    list->count = 0;
}


// Parses a single command line string into a cmd_entry_t structure
// Returns a dynamically allocated cmd_entry_t* or NULL on error/empty line
cmd_entry_t *parse_command_line(char *line) {
    char *rest = line;
    char *token;
    char *saveptr;

    // Skip leading whitespace
    while (*rest && (*rest == ' ' || *rest == '\t')) {
        rest++;
    }

    if (*rest == '\0' || *rest == '#') { // Empty line or comment
        return NULL;
    }

    cmd_entry_t *cmd = (cmd_entry_t *)malloc(sizeof(cmd_entry_t));
    if (!cmd) {
        syslog_critical("Failed to allocate cmd_entry_t");
        return NULL;
    }
    memset(cmd, 0, sizeof(cmd_entry_t));
    cmd->type = CMD_TYPE_ONCE; // Default type
    cmd->pid = 0;
    cmd->active = false;
    cmd->spawned_once = false;

    // Make a copy of the line to tokenize, as strtok_r modifies the string
    char *line_copy = strdup(line);
    if (!line_copy) {
        syslog_critical("Failed to duplicate line for parsing");
        free(cmd);
        return NULL;
    }
    rest = line_copy;

    // First token: "once", "repeat", or executable path
    token = strtok_r(rest, " \t\n", &saveptr);
    if (!token) {
        free(line_copy);
        cmd_destroy(cmd); // Free partially allocated Cmd
        return NULL;
    }

    if (strcmp(token, "once") == 0) {
        cmd->type = CMD_TYPE_ONCE;
        token = strtok_r(NULL, " \t\n", &saveptr); // Get next token (path)
    } else if (strcmp(token, "repeat") == 0) {
        cmd->type = CMD_TYPE_REPEAT;
        token = strtok_r(NULL, " \t\n", &saveptr); // Get next token (path)
    }

    if (!token) { // No path after 'once'/'repeat' or empty line
        free(line_copy);
        cmd_destroy(cmd);
        return NULL;
    }

    // This token must be the path
    strncpy(cmd->path, token, MAX_PATH_LEN - 1);
    cmd->path[MAX_PATH_LEN - 1] = '\0';

    // Add path to argv[0]
    cmd->argv[cmd->argc] = strdup(cmd->path);
    if (!cmd->argv[cmd->argc]) {
        syslog_critical("Failed to strdup path for argv");
        free(line_copy);
        cmd_destroy(cmd);
        return NULL;
    }
    cmd->argc++;

    // Remaining tokens are arguments
    while ((token = strtok_r(NULL, " \t\n", &saveptr)) != NULL) {
        if (cmd->argc >= MAX_ARGS) {
            syslog_info("Too many arguments for command %s, truncating.", cmd->path);
            break;
        }
        cmd->argv[cmd->argc] = strdup(token);
        if (!cmd->argv[cmd->argc]) {
            syslog_critical("Failed to strdup argument");
            free(line_copy);
            cmd_destroy(cmd);
            return NULL;
        }
        cmd->argc++;
    }
    cmd->argv[cmd->argc] = NULL; // NULL-terminate argv

    free(line_copy); // Free the duplicated line
    return cmd;
}


void fatal(char *msg) {
    syslog_critical(msg);
    for(;;);
}

char *load_initrc_file() {
    int h = open("/etc/initrc");
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

cmd_list_t *load_and_parse_initrc_commands() {
    char *text = load_initrc_file();
    if (text == NULL) {
        syslog_info("Error loading /etc/initrc file");
        return NULL;
    }

    cmd_list_t *commands = (cmd_list_t *)malloc(sizeof(cmd_list_t));
    if (!commands) {
        syslog_critical("Failed to allocate cmd_list_t");
        free(text);
        return NULL;
    }
    cmd_list_init(commands);

    char *start = text;
    char *saveptr_line;
    char *line = strtok_r(start, "\n", &saveptr_line);

    while (line != NULL) {
        cmd_entry_t *cmd = parse_command_line(line);
        if (cmd != NULL) {
            cmd_list_add(commands, cmd);
        }
        line = strtok_r(NULL, "\n", &saveptr_line);
    }

    free(text); // Free the buffer loaded from initrc file
    return commands;
}


// Function to spawn a command from a cmd_entry_t
pid_t spawn_command(cmd_entry_t *cmd) {
    char *envp[] = { NULL }; // No environment variables for now
    pid_t pid = spawn(cmd->path, cmd->argv, envp);
    if (pid < 0) {
        syslog_error("Failed to spawn %s", cmd->path);
    } else {
        syslog_info("Spawned %s with pid %d", cmd->path, pid);
        cmd->pid = pid;
        cmd->active = true;
        cmd->spawned_once = true; // Mark as spawned for 'once' commands
    }
    return pid;
}

int main(int argc, char *argv[]) {
    syslog_info("init running...");

    cmd_list_t *init_commands = load_and_parse_initrc_commands();
    if (init_commands == NULL) {
        syslog_critical("Failed to load and parse initrc commands, but continuing...");
        // Continue even if parsing failed, maybe initrc is empty or corrupted
        init_commands = (cmd_list_t *)malloc(sizeof(cmd_list_t));
        if (!init_commands) fatal("Failed to allocate empty cmd_list_t");
        cmd_list_init(init_commands);
    }
    
    // Initial spawning of commands
    cmd_list_node_t *current = init_commands->head;
    while (current != NULL) {
        spawn_command(current->command);
        current = current->next;
    }

    // Main event loop for process management
    while (true) {

        // Check for exited children
        int status;
        syslog_info("init calling wait()");
        pid_t exited_pid = wait(&status);
        syslog_info("init calling wait() returned");

        if (exited_pid < 0) {
            syslog_debug("wait() --> %d, will sleep and wait for reparented children in the future", exited_pid);
            sleep(3000);
            continue;
        }

        if (exited_pid > 0) {
            syslog_info("Child with pid %d exited with status %d", exited_pid, status);

            // Find the command that exited
            cmd_list_node_t *node = init_commands->head;
            while (node != NULL) {
                if (node->command->pid == exited_pid) {
                    node->command->active = false; // Mark as inactive

                    if (node->command->type == CMD_TYPE_REPEAT) {
                        syslog_info("Respawning repeated command %s", node->command->path);
                        spawn_command(node->command); // Respawn
                    }
                    break;
                }
                node = node->next;
            }
        }
    }

    // This part should ideally never be reached in an init process
    syslog_critical("init is terminating unexpectedly");
    cmd_list_destroy(init_commands);
    free(init_commands);
    return 0;
}
