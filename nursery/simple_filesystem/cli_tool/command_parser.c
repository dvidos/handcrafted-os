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

        // Check for --help or -h
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            return PARSE_SHOW_HELP;
        }

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
                error("Memory allocation failed for options.");
                return PARSE_ERROR;
            }
            parsed_option *p_opt = &opts->options[opts->option_count++];
            p_opt->name = opt_conf->value_name;
            
            if (opt_conf->has_argument) {
                arg_index++;
                if (arg_index >= argc) {
                    error("Option %s requires an argument.", opt_conf->long_name);
                    return PARSE_ERROR;
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
    return PARSE_OK;
}

// --- Main Dispatch Function ---

int parse_and_dispatch_commands(int argc, char *argv[], const command_config *all_commands, size_t num_all_commands, void (*print_general_help_func)()) {
    if (argc < 2) { // Need at least "sfs" and a command or "help"
        print_general_help_func();
        return 1;
    }

    const char *command_name = argv[1]; // Initially assume argv[1] is the command

    // Special case: "sfs help" or "sfs help <target_command>"
    if (strcmp(command_name, "help") == 0) {
        const command_config *help_command_config = find_command("help", all_commands, num_all_commands);
        if (!help_command_config) {
            return error("Internal Error: 'help' command definition not found.");
        }
        
        command_options dummy_opts = {0}; // help command has no options
        if (argc == 2) { // Just "sfs help"
            print_general_help_func();
            return 0;
        } else { // "sfs help <target_command>"
            const char *target_command_name = argv[2];
            char *help_argv[] = {(char*)target_command_name}; // Pass target_command_name as argument to execute_help
            int result = help_command_config->execute(&dummy_opts, 1, help_argv);
            free(dummy_opts.options);
            return result;
        }
    }
    
    // Regular commands: "sfs <command> ..." or "sfs <command> help"
    const command_config *command = find_command(command_name, all_commands, num_all_commands);
    if (command == NULL) {
        print_general_help_func();
        return error("Unknown command '%s'", command_name);
    }

    // Check for "sfs <command> help" (positional argument 'help')
    if (argc > 2 && strcmp(argv[2], "help") == 0) {
        const command_config *help_command_config = find_command("help", all_commands, num_all_commands);
        if (help_command_config) {
            command_options dummy_opts = {0};
            char *help_argv[] = {(char*)command_name};
            int result = help_command_config->execute(&dummy_opts, 1, help_argv);
            free(dummy_opts.options);
            return result;
        }
    }


    command_options opts = {0};
    int non_option_argc = 0;
    char **non_option_argv = malloc(argc * sizeof(char*));
    if (!non_option_argv) {
        error("Memory allocation failed for non-option arguments.");
        return 1;
    }

    // Pass argv + 2 (skip "sfs" and "command_name")
    int parse_status = parse_options(command, argc - 2, argv + 2, &opts, &non_option_argc, non_option_argv);
    
    if (parse_status == PARSE_SHOW_HELP) {
        // User explicitly asked for help on this command
        const command_config *help_command_config = find_command("help", all_commands, num_all_commands);
        if (help_command_config) {
            command_options dummy_opts = {0};
            char *help_argv[] = {(char*)command_name};
            int result = help_command_config->execute(&dummy_opts, 1, help_argv);
            free(dummy_opts.options);
            free(non_option_argv);
            free(opts.options);
            return result;
        } else {
            error("Internal Error: 'help' command definition not found.");
            free(non_option_argv);
            free(opts.options);
            return 1;
        }
    } else if (parse_status == PARSE_ERROR) {
        // An error occurred during option parsing (already printed by parse_options)
        free(non_option_argv);
        free(opts.options);
        return 1;
    }

    // Check for missing mandatory positional arguments
    int mandatory_args_count = 0;
    if (command->args) {
        for (int i = 0; command->args[i].name; i++) {
            if (!command->args[i].is_optional) {
                mandatory_args_count++;
            }
        }
    }

    if (non_option_argc < mandatory_args_count) {
        error("Missing mandatory argument(s) for command '%s'.", command_name);
        const command_config *help_command_config = find_command("help", all_commands, num_all_commands);
        if (help_command_config) {
            command_options dummy_opts = {0};
            char *help_argv[] = {(char*)command_name};
            help_command_config->execute(&dummy_opts, 1, help_argv);
            free(dummy_opts.options);
        }
        free(non_option_argv);
        free(opts.options);
        return 1;
    }

    // Check for missing mandatory options
    if (command->options) {
        for (int i = 0; command->options[i].long_name; i++) {
            const option_config *opt_conf = &command->options[i];
            if (opt_conf->is_mandatory) {
                bool found = false;
                for (int j = 0; j < opts.option_count; j++) {
                    if (strcmp(opts.options[j].name, opt_conf->value_name) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    error("Missing mandatory option '%s' for command '%s'.", opt_conf->long_name, command_name);
                    const command_config *help_command_config = find_command("help", all_commands, num_all_commands);
                    if (help_command_config) {
                        command_options dummy_opts = {0};
                        char *help_argv[] = {(char*)command_name};
                        help_command_config->execute(&dummy_opts, 1, help_argv);
                        free(dummy_opts.options);
                    }
                    free(non_option_argv);
                    free(opts.options);
                    return 1;
                }
            }
        }
    }
    
    // Call execute
    int result = command->execute(&opts, non_option_argc, non_option_argv);

    free(opts.options);
    free(non_option_argv);

    return result;
}