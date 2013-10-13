#include "lur.h"
#include "args.h"
#include "freeid.h"
#include "hashid.h"
#include "gfreeid.h"
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h> 
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

void test_freeid() {
    struct freeid fi;
    freeid_init(&fi, 3, 5);

    assert(freeid_alloc(&fi, -1) == -1);
    assert(freeid_alloc(&fi, 6) == -1);
    assert(freeid_alloc(&fi, 100) == -1);

    int i;
    for (i=0; i<3; ++i) {
        assert(freeid_alloc(&fi, i) == i);
    }
    assert(freeid_alloc(&fi, 3) == -1);
    assert(freeid_free(&fi, 2) == 2);
    assert(freeid_alloc(&fi, 3) == 2);
    assert(freeid_free(&fi, 0) == 0);
    assert(freeid_free(&fi, 1) == 1);
    assert(freeid_free(&fi, 3) == 2);
    assert(freeid_free(&fi, 2) == -1);
    assert(freeid_alloc(&fi, 4) == 2);

    freeid_fini(&fi);
}

void test_hashid() {
    struct hashid fi;
    hashid_init(&fi, 3, 5); // 8

    assert(hashid_alloc(&fi, 0) == 0);
    assert(hashid_alloc(&fi, 1) == 1);
    assert(hashid_alloc(&fi, 2) == 2);
    assert(hashid_alloc(&fi, 3) == -1);
    assert(hashid_free(&fi, 0) == 0);
    assert(hashid_free(&fi, 4) == -1);
    assert(hashid_free(&fi, 1) == 1);
    assert(hashid_free(&fi, 2) == 2);

    assert(hashid_alloc(&fi, 123) == 2);
    assert(hashid_alloc(&fi, 124) == 1);
    assert(hashid_alloc(&fi, 125) == 0);
    assert(hashid_alloc(&fi, 126) == -1);
  
    assert(hashid_free(&fi, 123) == 2);
    assert(hashid_free(&fi, 124) == 1);
    assert(hashid_free(&fi, 125) == 0);

    assert(hashid_alloc(&fi, 125) == 0);
    assert(hashid_alloc(&fi, 133) == 1);
    assert(hashid_alloc(&fi, 141) == 2);
    assert(hashid_alloc(&fi, 142) == -1);

    assert(hashid_free(&fi, 142) == -1);
    assert(hashid_free(&fi, 125) == 0);
    assert(hashid_free(&fi, 133) == 1);
    assert(hashid_free(&fi, 141) == 2);
    hashid_fini(&fi);
}

struct idtest{
    int id;
    int used;
};
struct gftest {
    GFREEID_FIELDS(idtest);
};
void test_gfreeid() {
    struct gftest gf;
    GFREEID_INIT(idtest, &gf, 1);
    struct idtest* i1 = GFREEID_ALLOC(idtest, &gf);
    assert(i1-gf.p == 0);
    assert(gf.cap == 1);
    struct idtest* i2 = GFREEID_ALLOC(idtest, &gf);
    assert(i2-gf.p == 1);
    assert(gf.cap == 2);
    struct idtest* i3 = GFREEID_ALLOC(idtest, &gf);
    assert(i3-gf.p == 2);
    assert(gf.cap == 4);
    GFREEID_FREE(idtest, &gf, GFREEID_SLOT(&gf, 0));
    GFREEID_FREE(idtest, &gf, GFREEID_SLOT(&gf, 1));
    GFREEID_FREE(idtest, &gf, GFREEID_SLOT(&gf, 2));
    GFREEID_FINI(idtest, &gf);
}

int 
main(int argc, char* argv[]) {
    //test_lur();
    //test_args();
    //test_union();
    test_freeid();
    test_hashid();
    test_gfreeid();
    return 0;
}
