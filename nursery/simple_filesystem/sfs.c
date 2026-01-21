#include <stdlib.h>
#include <stdio.h>



int general_help() {
    printf("sfs - a tool to work with a Simple File System file\n");
    printf("\n");
    printf("Syntax: sfs [options] [cmds] [args]\n");
    printf("  Options:\n");
    printf("    -v volume-file        File to be used for volume\n");
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

int main(int argc, char *argv[]) {
    general_help();
    return 0;
}
