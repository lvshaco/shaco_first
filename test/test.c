#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "lur.h"
#include "args.h"
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

struct Test {
    struct {
        int i;
    }v [3];
};

void
test_lur() {
    struct Test t;
    t.v[1].i = 1;
    struct lur* L = lur_create();
    const char* r = lur_dofile(L, "config.lua", "shaco");
    if (!LUR_OK(r)) {
        printf("%s", r);
        return;
    }
    printf("%f\n", lur_getfloat(L, "f", 0));
    printf("%s\n", lur_getstr(L, "s", ""));
    printf("%d\n", lur_getint(L, "t1.i", 0));
    printf("%s\n", lur_getstr(L, "t1.s", ""));
    printf("%d\n", lur_getint(L, "t1.tt1.a", 0));
    printf("%s\n", lur_getstr(L, "t1.tt1.b", ""));
    printf("%d\n", lur_getint(L, "t1.tt1.ttt1.k", 0));
    printf("%s\n", lur_getstr(L, "t1.tt1.ttt1.v", ""));
    printf("----------------------\n");
    int next = lur_getnode(L, "t2");
    while (next) {
        printf("%d\n", lur_getint(L, "a", 0));
        printf("%d\n", lur_getint(L, "b", 0));
        printf("%s\n", lur_getstr(L, "c", ""));
        next = lur_nextnode(L);
    }
    lur_free(L);
}

void
test_args() {
    const char* strv[] = {
    "abc",
    "abc def",
    "abc def 123 456 789 012 345 678 901 234",
    "abc def 123 456 789 012 345 678 901 234 567 890",
    "  abc    123  ",
    "  abc  ",
    "  abc",
    "abc  ",
    };

    struct args p;
    int i;
    int n;
    printf("=====test_args input data=====\n");
    for (i=0; i<sizeof(strv)/sizeof(strv[0]); ++i) {
        printf("%s\n", strv[i]);
    }
    printf("=====test args_parsestrl=====\n");
    for (i=0; i<sizeof(strv)/sizeof(strv[0]); ++i) {
        args_parsestrl(&p, 3, strv[i], strlen(strv[i]));
        printf(">> %d: argc=%d\n", i, p.argc);
        for (n=0; n< p.argc; ++n) {
            printf("%s,", p.argv[n]);
        }
        printf("\n");
    }
    printf("=====test args_parsestr=====\n");
    for (i=0; i<sizeof(strv)/sizeof(strv[0]); ++i) {
        args_parsestr(&p, 0, strv[i]);
        printf(">> %d: argc=%d\n", i, p.argc);
        for (n=0; n< p.argc; ++n) {
            printf("%s,", p.argv[n]);
        }
        printf("\n");
    }

}

struct ID {
    union {
        struct {
            uint16_t tid:6;
            uint16_t sid:10;
        };
        uint16_t id;
    };
};
void test_union() {
    struct ID id;
    int i,i2;
    scanf("%u %u", &i, &i2);
    id.tid = i;
    id.sid = i2;
    printf("size:%d, %u,%u,%u\n", sizeof(id), id.tid, id.sid, id.id);
}
int 
main(int argc, char* argv[]) {
    //test_lur();
    //test_args();
    test_union();
    return 0;
}
