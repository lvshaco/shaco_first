#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "lur.h"

/*
void 
host_log(int level, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char log[1024];
    int n = vsnprintf(log, sizeof(log), fmt, ap);
    va_end(ap);
    if (n < 0) {
        return; // output error
    }
    if (n >= sizeof(log)) {
        // truncate
    }
    // notify service_log handle
    //printf("n %d.\n", n);
    //printf(log);
}
*/

void
test_lur() {
    struct lur* L = lur_create();
    const char* r = lur_dofile(L, "config.lua");
    if (!LUR_OK(r)) {
        printf(r);
        return -1;
    }
    printf("%d\n", lur_getint(L, "i", 0));
    printf("%s\n", lur_getstr(L, "s", ""));
    printf("%d\n", lur_getint(L, "t1.i", 0));
    printf("%s\n", lur_getstr(L, "t1.s", ""));
    printf("%d\n", lur_getint(L, "t1.tt1.a", 0));
    printf("%s\n", lur_getstr(L, "t1.tt1.b", ""));
    printf("%d\n", lur_getint(L, "t1.tt1.ttt1.k", 0));
    printf("%s\n", lur_getstr(L, "t1.tt1.ttt1.v", ""));
    printf("%s\n", lur_getstr(L, "t1.tt1..v", "")); 
    printf("%s\n", lur_getstr(L, "t1.tt1..v.", "")); 
    printf("%s\n", lur_getstr(L, ".v.", "")); 
    lur_free(L);
}

int 
main(int argc, char* argv[]) {
    test_lur();
    return 0;
}
