#include "command_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // For atol
#include "utils.h" // For error function

// --- Accessor Functions for command_options ---

const char* get_str_option(command_options *opts, const char *name) {
    for (int i = 0; i < opts->option_count; i++) {
        if (strcmp(opts->options[i].name, name) == 0) {
            return opts->options[i].str_value;
        }
    }
    return NULL;
}

long get_int_option(command_options *opts, const char *name, long default_value) {
    for (int i = 0; i < opts->option_count; i++) {
        if (strcmp(opts->options[i].name, name) == 0) {
            return opts->options[i].int_value;
        }
    }
    return default_value;
}

bool get_bool_option(command_options *opts, const char *name) {
    for (int i = 0; i < opts->option_count; i++) {
        if (strcmp(opts->options[i].name, name) == 0) {
            return opts->options[i].bool_value;
        }
    }
    return false;
}

// --- Static Helper Functions for Parsing ---

const command_config* find_command(const char *command_name, const command_config *all_commands, size_t num_all_commands) {
    for (size_t i = 0; i < num_all_commands; i++) {
        if (all_commands[i].name == NULL) continue; // Skip NULL terminator
        if (strcmp(all_commands[i].name, command_name) == 0) {
            return &all_commands[i];
        }
    }
    return NULL;
}

static int parse_options(const command_config *command, int argc, char *argv[], command_options *opts, int *non_option_argc, char **non_option_argv) {
    int arg_index = 0;
    while(arg_index < argc) {
        const char* arg = argv[arg_index];
        const option_config *opt_conf = NULL;

        if (command->options) {
            for (int i = 0; command->options[i].long_name; i++) {
                if (strcmp(arg, command->options[i].long_name) == 0 || (arg[0] == '-' && arg[1] == command->options[i].short_name && arg[2] == '\0')) {
                    opt_conf = &command->options[i];
                    break;
                }
            }
        }

        if (opt_conf) {
            opts->options = realloc(opts->options, (opts->option_count + 1) * sizeof(parsed_option));
            if (!opts->options) {
                return error("Memory allocation failed for options.");
            }
            parsed_option *p_opt = &opts->options[opts->option_count++];
            p_opt->name = opt_conf->value_name;
            
            if (opt_conf->has_argument) {
                arg_index++;
                if (arg_index >= argc) {
                    return error("Option %s requires an argument.", opt_conf->long_name);
                }
                char *val = argv[arg_index];
                if (opt_conf->type == OPT_STRING) p_opt->str_value = val;
                else if (opt_conf->type == OPT_INT) p_opt->int_value = atol(val);
            } else { // bool
                p_opt->bool_value = true;
            }
        } else {
            non_option_argv[(*non_option_argc)++] = argv[arg_index];
        }
        arg_index++;
    }
    return 0;
}

// --- Main Dispatch Function ---

int parse_and_dispatch_commands(int argc, char *argv[], const command_config *all_commands, size_t num_all_commands, void (*print_general_help_func)()) {
    if (argc < 2) { // Need at least "sfs" and a command or "help"
        print_general_help_func();
        return 1;
    }

    // Special case 1: "sfs help" (general help)
    if (argc == 2 && strcmp(argv[1], "help") == 0) {
        print_general_help_func();
        return 0;
    }

    // Special case 2: "sfs help <target_command>" (specific help)
    if (strcmp(argv[1], "help") == 0) {
        if (argc < 3) { // "sfs help" needs a target command
            print_general_help_func(); // Print usage because it's an incomplete help command
            return error("'sfs help' requires a command name.");
        }
        const char *target_command_name = argv[2];
        const command_config *help_command_config = find_command("help", all_commands, num_all_commands);
        if (help_command_config) {
            command_options dummy_opts = {0}; // help command has no options
            char *help_argv[] = {(char*)target_command_name}; // Pass target_command_name as argument to execute_help
            // image_file is NULL for help command, as it doesn't operate on an image
            int result = help_command_config->execute(NULL, &dummy_opts, 1, help_argv);
            free(dummy_opts.options); // Should be NULL anyway, but good practice
            return result;
        } else {
            return error("Internal Error: 'help' command definition not found.");
        }
    }
    
    // Regular commands: "sfs <image_file> <command> ..."
    if (argc < 3) { // Need at least "sfs <image_file> <command>"
        print_general_help_func();
        return 1;
    }

    const char *image_file = argv[1];
    const char *command_name = argv[2];
    
    const command_config *command = find_command(command_name, all_commands, num_all_commands);
    if (command == NULL) {
        print_general_help_func();
        return error("Unknown command '%s'", command_name);
    }

    command_options opts = {0};
    int non_option_argc = 0;
    char **non_option_argv = malloc(argc * sizeof(char*));
    if (!non_option_argv) {
        return error("Memory allocation failed for non-option arguments.");
    }

    // Pass argv + 3 (skip "sfs", "image_file", "command_name")
    if (parse_options(command, argc - 3, argv + 3, &opts, &non_option_argc, non_option_argv) != 0) {
        free(non_option_argv);
        free(opts.options);
        return 1; // Error already printed by parse_options
    }
    
    int result = command->execute(image_file, &opts, non_option_argc, non_option_argv);

    free(opts.options);
    free(non_option_argv);

    return result;
}