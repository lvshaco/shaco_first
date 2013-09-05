#include <stdio.h>
#include "host.h"

int 
main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("usage: %s config (lua file)\n", argv[0]);
        return 1;
    }
    const char* file = argv[1];

    if (host_create(file) == 0) {
        host_start();
        host_free();
    }
    return 0;
}
