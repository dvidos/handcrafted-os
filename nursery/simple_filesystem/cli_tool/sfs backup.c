#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

/*
    This tool to support making disk images in files, 
    for booting purposes, supporting:
    ----------------------------------------------
    1. raw sectors (for boot leaders & kernels)
    2. SFS Filesystem at some partition 
    3. Legacy BIOS partition table editing (1-4 partitions)
    4. byte/word/dword edit in file (e.g. for setting load addresses)

    General syntax will be "sfs <command> <subcommand> arg0 arg1 ..."
    Command "help" is the default
*/

#define OK     0
#define ERROR  1

int error(char *fmt, ...) {
    fprintf(stderr, "Error: ");
    va_list vl;
    va_start(vl, fmt);
    vfprintf(stderr, fmt, vl);
    va_end(vl);
    return ERROR;
}

typedef struct option_config option_config;
typedef struct arg_config arg_config;
typedef struct command_config command_config;
typedef struct command_options command_options;
typedef struct command_args command_args;
typedef int (command_exec_func)(command_options *opts, command_args *args);

struct option_config {
    char *name;      // e.g. "-v" or "--volume"
    char *description;
    bool has_argument;     // e.g. needs an argument or is bool
    bool numeric_argument; // is numeric argument or string
};

struct arg_config {
    char *name;      // e.g. "-v" or "--volume"
    char *description;
    bool mandatory;
};

option_config num_option(char *name, char *description);
option_config str_option(char *name, char *description);
option_config bool_option(char *name, char *description);
option_config *options_array(int num, ...);

arg_config mandatory_arg(char *name, char *description);
arg_config optional_arg(char *name, char *description);
arg_config *args_array(int num, ...);

command_config command_group(char *name, char *description, option_config *options, command_config *subcommands);
command_config command(char *name, char *description, option_config *options, command_exec_func *exec);
command_config *commands_array(int num, ...);

typedef struct command_config {
    char *name;
    char *brief_description;
    int min_args;
    int max_args;
    int (*execute)(command_options *options, command_args *args);
    option_config **options;
    arg_config **options;
    command_config **children;
} command_config;

typedef struct command_options {
    char **names;
    char **values;
    int count;
    char *(*get_str_option)(command_options *co, char *name, char *default_value);
    int (*get_int_option)(command_options *co, char *name, int default_value);
    bool (*get_bool_option)(command_options *co, char *name);
} command_options;

typedef struct command_args {
    char **values;
    int count;
} command_args;

// -------------------------------------------------

void print_command_syntax(command_config *cmd) {
    char buffer[64];
    printf("  Syntax: %s    %s\n", cmd->name, cmd->brief_description);
    if (cmd->options != NULL) {
        printf("  Options:\n");
        for (option_config *option = cmd->options; option; option++) {
            sprintf(buffer, "%s%s%s", option, option->has_argument ? (option->numeric_argument ? "num" : "str") : "", option->description);
            printf("    %-20s  %s\n", buffer, option->description);
        }
    }
    if (cmd->children != NULL) {
        printf("  Commands:\n");
        for (command_config *child = cmd->children; child; child++) {
            printf("    %-20s  %s\n", cmd->name, child->brief_description);
        }
    }
};

int demo_import_file(command_options *opts, command_args *args) {
    char *volume_file = opts->get_str_option(opts, "v", "");
    if (strlen(volume_file) == 0)
        return error("No volume file defined");
    char *host_file_name = args->values[0];
    char *volume_file_name = args->values[1];
    // fopen() in linux, sfs->open() in volume
    // copy
    // close both

}

// ------------------------------------------------

int parse_and_execute(int argc, char *argv[], command_options *root_command) {
    // given the tree of commands, options, arguments, 
    // parse argc/argv, validate and prepare command args and options
    // and call its function
    // if nothing is found, or command is "help", or option "-h", "--help", present syntax based on description
}

void general_help() {
    printf("sfs - a tool to work with a Simple File System file\n");
    printf("\n");
    printf("Syntax: sfs [options] [cmds] [args]\n");
    printf("  Options:\n");
    printf("    -v volume-file        File to be used for volume\n");
    printf("    -r sect-count         Reserved secrtors before superblock\n");
    printf("  Commands:\n");
    printf("    create      Create volume file, arg: size\n");
    printf("    describe    List contents of volume file\n");
    printf("    ls          List dir contents in volume, arg: dir\n");
    printf("    put         Put file from host into volume, args: hostfile, target-dir\n");
    printf("    get         Get file form volume into host, arg: vol-path\n");
    printf("    rm          Delete file in volume, arg: vol-path\n");
    printf("    mkdir       Create dir in volume, arg: vol-path\n");
    printf("    secw        Write sector in volume, args: sec-num, contents\n");
    printf("    secr        Read sector in volume, args: sec-num\n");
    printf("    secz        Zap (wipe) sector in volume, args: sec-num\n");
}

static int _volume_create(command_options *opts, command_args *args) { return 1; }
static int _volume_import_sectors(command_options *opts, command_args *args) { return 1; }
static int _volume_mkfs(command_options *opts, command_args *args) { return 1; }
static int _volume_mkdir(command_options *opts, command_args *args) { return 1; }
static int _volume_import_file(command_options *opts, command_args *args) { return 1; }
static int _volume_info(command_options *opts, command_args *args) { return 1; }

int main(int argc, char *argv[]) {
    // create, copy into sector(s), mkfs, ls, mkdir, import

    command_config main_commands[] = {
        command("create", "Creates a new volume", options_array(), _volume_create),
        command("wrsect", "Writes a sector", options_array(), )
    };
    command_config sfs = command_group(
        "sfs", "Manipulate Simple File Sys volumes",
        str_option("-v", "Volume file, e.g. disk.img"),
        
    )

    };


    general_help();
    return 0;
}
