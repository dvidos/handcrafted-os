#pragma once
#include <stdbool.h>
#include <stddef.h> // For size_t, required by execute signature

// --- Data Structures for Parsing ---

typedef enum { OPT_STRING, OPT_INT, OPT_BOOL } option_type;

#define PARSE_OK          0
#define PARSE_SHOW_HELP   1
#define PARSE_ERROR       2

typedef struct {
    const char *long_name;
    char short_name;
    const char *description;
    bool has_argument;
    const char *value_name; // The key for the parsed option
    option_type type;
    bool is_mandatory;
} option_config;

typedef struct {
    const char *name;
    char *str_value;
    long int_value;
    bool bool_value;
} parsed_option;

typedef struct {
    parsed_option *options;
    int option_count;
} command_options;

// Accessor functions for command_options
const char* get_str_option(command_options *opts, const char *name);
long get_int_option(command_options *opts, const char *name, long default_value);
bool get_bool_option(command_options *opts, const char *name);

// Command configuration structure (must be defined before command_config in command_parser.c)
typedef struct {
    const char *name;         // e.g., "<path>"
    const char *description;  // e.g., "The path to the directory to create"
    bool is_optional;
} arg_config;

typedef struct command_config {
    const char *name;
    const char *description;
    int (*execute)(command_options *opts, int argc, char *argv[]);
    const option_config *options;
    const arg_config *args;       // Describes positional arguments
} command_config;

// Main dispatch function
int parse_and_dispatch_commands(int argc, char *argv[], const command_config *all_commands, size_t num_all_commands, void (*print_general_help_func)());

// Function to find a command
const command_config* find_command(const char *command_name, const command_config *all_commands, size_t num_all_commands);
