#include "lur.h"
#include "args.h"
#include "freeid.h"
#include "hashid.h"
#include "gfreeid.h"
#include "freelist.h"
#include "redis.h"
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h> 
#include <stdlib.h>
#include <time.h>

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

static void
_test_redisstep(struct redis_reply* reply, int initstep, int random) {
    int r;
    struct redis_reader* reader = &reply->reader;
    redis_resetreply(reply); 
    const char* tmp = "*2\r\n"
        "*5\r\n+OK\r\n$6\r\nlvxiao\r\n-UNKNOW ERROR\r\n:1\r\n$1\r\nJ\r\n"
        "*3\r\n$3\r\nabc\r\n$6\r\nlvxiao\r\n$4\r\n1234\r\n";
    int sz = strlen(tmp);
    int i;
    int step = initstep;
    for (i=0; i<sz; i+=step) {
        if (random) {
            step = rand() % initstep + 1;
            printf("%d ", step);
        }
        if (i+step > sz) {
            step = sz-i;
        }
        strncpy(&reader->buf[reader->sz], tmp+i, step);
        reader->sz += step;
        r = redis_getreply(reply);
        if (i+step < sz) {
            assert(r == REDIS_NEXTTIME);
        } else {
            assert(r == REDIS_SUCCEED);
            redis_walkreply(reply);
        }
    }
}

void test_redis() {
    struct redis_reply reply;
    redis_initreply(&reply, 512);
    struct redis_reader* reader = &reply.reader;
    const char* tmp;
    int sz;
    int r;
    int i;
    // 1
    tmp = "$6\r\nfoobar\r\n";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_SUCCEED);
    redis_walkreply(&reply);

    // 2
    redis_resetreply(&reply); 

    tmp = "+OK\r\n";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_SUCCEED);
    redis_walkreply(&reply);

    // 3
    redis_resetreply(&reply); 

    tmp = "+QUEUED\r\n";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_SUCCEED);
    redis_walkreply(&reply);

    // 4
    redis_resetreply(&reply); 

    tmp = "-ERROR\r\n";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_SUCCEED);
    redis_walkreply(&reply);

    // 5
    redis_resetreply(&reply); 

    tmp = "-WALK unknown command\r\n";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_SUCCEED);
    redis_walkreply(&reply);

    // 6
    redis_resetreply(&reply); 

    tmp = ":12345\r\n";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_SUCCEED);
    redis_walkreply(&reply);

    // 7
    redis_resetreply(&reply); 

    tmp = ":0\r\n";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_SUCCEED);
    redis_walkreply(&reply);

    // 8
    redis_resetreply(&reply); 

    tmp = "$-1\r\n";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_SUCCEED);
    redis_walkreply(&reply);

    // 8
    redis_resetreply(&reply); 

    tmp = "$0\r\n";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_SUCCEED);
    redis_walkreply(&reply);

    // 9
    redis_resetreply(&reply); 

    tmp = "*1\r\n$3\r\nabc\r\n";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_SUCCEED);
    redis_walkreply(&reply);

    // 10
    redis_resetreply(&reply); 

    tmp = "*3\r\n$3\r\nabc\r\n$6\r\nlvxiao\r\n$4\r\n1234\r\n";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_SUCCEED);
    redis_walkreply(&reply);

    // 11
    redis_resetreply(&reply); 

    tmp = "*5\r\n+OK\r\n$6\r\nlvxiao\r\n-UNKNOW ERROR\r\n:1\r\n$1\r\nJ\r\n";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_SUCCEED);
    redis_walkreply(&reply);

    // 12
    redis_resetreply(&reply); 

    tmp = "*2\r\n"
        "*5\r\n+OK\r\n$6\r\nlvxiao\r\n-UNKNOW ERROR\r\n:1\r\n$1\r\nJ\r\n"
        "*3\r\n$3\r\nabc\r\n$6\r\nlvxiao\r\n$4\r\n1234\r\n";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_SUCCEED);
    redis_walkreply(&reply);
/*
    // REDIS_ERROR
    // 13
    redis_resetreply(&reply); 

    tmp = "*2YU\r\n("
        "*5\r\n+OK\r\n$6\r\nlvxiao\r\n-UNKNOW ERROR\r\n:1\r\n$1\r\nJ\r\n"
        "*3\r\n$3\r\nabc\r\n$6\r\nlvxiao\r\n$4\r\n1234\r\n";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_ERROR);
    redis_walkreply(&reply);
*/
    // REDIS_NEXTTIME
    // 14
    printf("-----------14\n");
    redis_resetreply(&reply); 

    tmp = "*2\r\n";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_NEXTTIME);

    tmp = "*5\r\n+OK\r\n$6\r\nlvxiao\r\n-UNKNOW ERROR\r\n:1\r\n$1\r\nJ\r\n";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_NEXTTIME);

    tmp = "*3\r\n$3\r\nabc\r\n$6\r\nlvxiao\r\n$4\r\n1234\r\n";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_SUCCEED);

    redis_walkreply(&reply);

    // 15
    printf("-------------------------15\n");
    for (i=1; i<100; ++i) {
        _test_redisstep(&reply, i, 0);
    }
  
    printf("-------------------------16\n");
    for (i=1; i<100; ++i) {
        srand(time(NULL) + 1);
        _test_redisstep(&reply, i, 1);
    }

    // 17
    printf("-----------17\n");
    redis_resetreply(&reply); 

    tmp = "*";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_NEXTTIME);

    tmp = "2\r";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_NEXTTIME);

    tmp = "\n";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_NEXTTIME);

    tmp = "*5\r";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_NEXTTIME);

    tmp = "\n+OK\r\n$6\r\nlvxiao\r\n-UNK";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_NEXTTIME);

    tmp = "NOW ERROR\r\n:1\r\n$1";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_NEXTTIME);

    tmp = "\r\nJ\r\n*3";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_NEXTTIME);
 
    tmp = "\r\n$3\r\nabc\r\n$6\r\nlvxiao\r\n$4\r\n12";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_NEXTTIME);

    tmp = "34\r\n$4\r\nabcd";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_SUCCEED);

    redis_walkreply(&reply);

    redis_resetreply(&reply);

    r = redis_getreply(&reply);
    assert(r == REDIS_NEXTTIME);

    tmp = "\r\n";
    sz = strlen(tmp);
    strncpy(&reader->buf[reader->sz], tmp, sz);
    reader->sz += sz;
    r = redis_getreply(&reply);
    assert(r == REDIS_SUCCEED);
    redis_walkreply(&reply);

    redis_finireply(&reply);
}

struct fldata {
    int tag;
};

struct flink {
    FREELIST_ENTRY(flink, fldata);
};

struct fltest {
    FREELIST(flink);
};

void test_freelist() {
    struct fltest fl;
    FREELIST_INIT(&fl);

    struct fldata d1;
    d1.tag = 1;
    FREELIST_PUSH(flink, &fl, &d1);
    FREELIST_POP(flink, &fl);

    struct fldata d2;
    d2.tag = 2;
    FREELIST_PUSH(flink, &fl, &d1);
    FREELIST_PUSH(flink, &fl, &d2);

    struct fldata* d;
    d = FREELIST_HEAD(flink, &fl); 
    {
        assert(d);
        assert(d->tag == 1);
        FREELIST_POP(flink, &fl);
    } 
    d = FREELIST_HEAD(flink, &fl);
    {
    assert(d2.tag == 2);
    assert(fl.sz == 2);
    FREELIST_POP(flink, &fl);
    }

    FREELIST_PUSH(flink, &fl, &d1);
    FREELIST_PUSH(flink, &fl, &d2);
    assert(fl.sz == 2);
    FREELIST_FINI(flink, &fl);
}

int 
main(int argc, char* argv[]) {
    //test_lur();
    //test_args();
    //test_freeid();
    //test_hashid();
    //test_gfreeid();
    test_redis();
    test_freelist();
    return 0;
}
