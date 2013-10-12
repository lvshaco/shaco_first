#include "host.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int 
main(int argc, char* argv[]) {
    const char* file = "config.lua";
    if (argc > 1) {
        file = argv[1];
    } 
    if (argc > 2) {
        if (strcmp(argv[2], "-d") == 0) {
            daemon(1, 1);
        }
    }
 
    if (host_create(file) == 0) {
        host_start();
        host_free();
    }
    return 0;
}
