#include <stdio.h>
#include "host.h"

int 
main(int argc, char* argv[]) {
    const char* file;
    if (argc < 2) {
        file = "config.lua";
    } else {
        file = argv[1];
    }
    if (host_create(file) == 0) {
        host_start();
        host_free();
    }
    return 0;
}
