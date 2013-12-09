#include "lur.h"
#include "sc_util.h"
#include "args.h"
#include "freeid.h"
#include "hashid.h"
#include "gfreeid.h"
#include "freelist.h"
#include "redis.h"
#include "map.h"
#include "hmap.h"
#include "elog_include.h"
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h> 
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static uint64_t
_elapsed() {
    struct timespec ti;
    clock_gettime(CLOCK_MONOTONIC, &ti);
    return ti.tv_sec * 1000 + ti.tv_nsec / 1000000;
}


/*
void 
sc_log(int level, const char* fmt, ...) {
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
    redis_initreply(&reply, 512, 16*1024);
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

void test_redisnew(int times) {
    struct redis_reply reply;
    redis_initreply(&reply, 512, 16*1024);
    struct redis_reader* reader = &reply.reader;
    const char* tmp;
    int sz;
    int r;
    //int i;
    int off = 0;
    // 1
    //tmp = "*0\r\n";
    tmp = "$6\r\nfoobar\r\n";
    sz = strlen(tmp);

    int allcount = REDIS_REPLYSPACE(&reply)/sz * times;
    int curcount = 0;

    uint64_t t1 = _elapsed();
    int n;
    for (n=0; n<times; ++n) {
        int space = REDIS_REPLYSPACE(&reply);
        assert(space >= sz);
        //printf("%d, space %d pos %d sz %d off %d\n", n, space, reader->pos, reader->sz, off);
        strncpy(&reader->buf[reader->sz], tmp+off, sz-off);
        reader->sz += sz-off;
        space -= sz-off;

        while (space >= sz) {
            strncpy(&reader->buf[reader->sz], tmp, sz);
            reader->sz += sz;
            space -= sz;
        }
        off = space;
        if (off) {
            strncpy(&reader->buf[reader->sz], tmp, off);
            reader->sz += off;
        }
          
        r = redis_getreply(&reply);
        while (r == REDIS_SUCCEED) {
            curcount ++;
            //redis_walkreply(&reply);
            redis_resetreply(&reply);;
            r = redis_getreply(&reply);
        }
        assert(r != REDIS_ERROR);
        if (r == REDIS_NEXTTIME) {
            //printf("nexttime: %d, %d\n", reply.reader.pos, reply.reader.pos_last);
        }
        redis_resetreply(&reply);
    } 
    uint64_t t2 = _elapsed();
    redis_finireply(&reply);
    printf("test redis new, replycount %d, mustcount %d, use time: %d\n", 
            curcount, allcount, (int)(t2-t1));
    assert(curcount == allcount);
}

struct fldata {
    int tag;
};

struct flink {
    struct flink* next;
    struct fldata data;
};

struct fltest {
    FREELIST(flink);
};

void test_freelist() {
    struct fltest fl;
    FREELIST_INIT(&fl);

    struct flink* d1, *d2;
    d1 = FREELIST_PUSH(flink, &fl, sizeof(struct flink));
    d1->data.tag = 1;
    FREELIST_POP(flink, &fl);

    d1 = FREELIST_PUSH(flink, &fl, sizeof(struct flink));
    d1->data.tag = 1;

    d2 = FREELIST_PUSH(flink, &fl, sizeof(struct flink));
    d2->data.tag = 2;

    struct flink* d;
    d = FREELIST_POP(flink, &fl); 
    {
        assert(d);
        assert(d->data.tag == 1);
    } 
    d = FREELIST_POP(flink, &fl);
    {
    assert(d->data.tag == 2);
    assert(fl.sz == 2);
    }
    d1 = FREELIST_PUSH(flink, &fl, sizeof(struct flink));
    d1->data.tag = 1;
    d2 = FREELIST_PUSH(flink, &fl, sizeof(struct flink));
    d2->data.tag = 2;
    assert(fl.sz == 2);
    FREELIST_FINI(flink, &fl);
}

struct mapvalue {
    uint32_t id;
    uint32_t value;
};

struct strvalue {
    char id[40];
    uint32_t value;
};

void _mapcb(const char* key, void* value, void* ud) {
    struct strvalue* v = value;
    static uint64_t n = 0;
    n += v->value;
}
void test_map() {
    srand(time(NULL));
    uint32_t i, j;
    uint32_t cap = 1000000;
    uint32_t randmod = 1000000;

    // generate data
    struct mapvalue* all = malloc(sizeof(struct mapvalue) * cap);
    memset(all, 0, sizeof(struct mapvalue) * cap);
  
    
    bool* rands = malloc(sizeof(bool) * randmod);
    memset(rands, 0, sizeof(bool) * randmod);

    uint64_t t1, t2;
    t1 = _elapsed(); 
    for (i=0; i<cap; ++i) {
        uint32_t r = rand() % randmod;
        while (rands[r]) {
            r = rand() % randmod;
        }
        rands[r] = true;

        all[i].id = r;
        all[i].value = r;
    }
    t2 = _elapsed();
    printf("generate data use time: %d\n", (int)(t2-t1));

    uint32_t init = 1024;
    struct idmap* idm = idmap_create(init);
    t1 = _elapsed();
    for (i=0; i<cap; ++i) {
        assert(idmap_find(idm, all[i].id) == NULL);
        idmap_insert(idm, all[i].id, &all[i]);
    }
    for (i=0; i<cap; ++i) {
        assert(idmap_remove(idm, all[i].id) == &all[i]);
    }
    t2 = _elapsed();
    printf("idmap use time: %d\n", (int)(t2-t1));
    
    //idmap_free(idm);

    t1 = _elapsed(); 
    struct strvalue* psv = malloc(sizeof(struct strvalue) * cap);
    for (i=0; i<cap; ++i) {
        psv[i].value = i+1;
        for (j=0; j<39; ++j) {
            psv[i].id[j] = rand()%127+1;
        }
        psv[i].id[39] = '\0';
    }
    t2 = _elapsed();
    printf("generate strmap value use time: %d\n", (int)(t2-t1));

    struct strmap* strm = strmap_create(init);
    t1 = _elapsed();
    for (i=0; i<cap; ++i) {
        assert(strmap_find(strm, psv[i].id) == NULL);
        strmap_insert(strm, psv[i].id, &psv[i]);
    }
    t2 = _elapsed();
    /*for (i=0; i<cap; ++i) {
        assert(strmap_remove(strm, psv[i].id) == &psv[i]);
    }*/
    //t2 = _elapsed();
    printf("strmap use time: %d\n", (int)(t2-t1));
    t1 = _elapsed();
    strmap_foreach(strm, _mapcb, NULL);
    t2 = _elapsed();
    printf("strmap foreach use time: %d\n", (int)(t2-t1));
    //strmap_free(strm);

    struct strhmap* m = strhmap_create(init);
    t1 = _elapsed();
    for (i=0; i<cap; ++i) {
        assert(strhmap_find(m, psv[i].id) == NULL);
        strhmap_insert(m, psv[i].id, &psv[i]);
    }
    t2 = _elapsed();
    /*for (i=0; i<cap; ++i) {
        void* p = strhmap_remove(m, psv[i].id);
        printf("i %u, remove %p\n", i, p);
        assert(p == &psv[i]);
    }*/
    //t2 = _elapsed();
    printf("strhmap use time: %d\n", (int)(t2-t1));
   
    t1 = _elapsed();
    strhmap_foreach(m, _mapcb, NULL);
    t2 = _elapsed();
    printf("strhmap foreach use time: %d\n", (int)(t2-t1));
/*
    extern uint64_t I;
    extern uint64_t I2;
    extern uint64_t MAXKK;
    printf("MAXKK %llu, I %llu, I2 %llu, A %f\n", 
            (unsigned long long int)MAXKK,
            (unsigned long long int)I, 
            (unsigned long long int)I2,
            (float)I/(float)I2
            ); 
            */
    strhmap_free(m);
}

void
test_elog1() {
    struct elog* el = elog_create("/home/lvxiaojun/log/testlog.log");
    elog_set_appender(el, &g_elog_appender_file);
    elog_append(el, "1234567890\n", 11);
    sleep(1);
    elog_append(el, "abc\n", 4);
    sleep(1);
    elog_append(el, "ABC", 3);
    sleep(1);
    elog_append(el, "DEFGHI", 3);
    sleep(1);
    elog_free(el);
}

void
test_elog2() {
    struct elog* el = elog_create("/home/lvxiaojun/log/testlog.log");
    elog_set_appender(el, &g_elog_appender_rollfile);
    struct elog_rollfile_conf conf;
    conf.file_max_num = 2;
    conf.file_max_size = 10;
    elog_appender_rollfile_config(el, &conf);
    elog_append(el, "8234567890\n", 11);
    sleep(0.1);
    elog_append(el, "abc\n", 4);
    sleep(0.1);
    elog_append(el, "ABC", 3);
    sleep(0.1);
    elog_append(el, "DEFGHI", 3);
    sleep(0.1);
    elog_free(el);
}

void 
test_elog3(int times) {
    uint64_t t1, t2; 
    char data[1024];
    //long sz;
    int i;
    for (i=0; i<sizeof(data); ++i) {
        data[i] = '0';
    }
    data[sizeof(data)-1] = '\0';

    struct elog* el = elog_create("/home/lvxiaojun/log/testfilelog.log");
    elog_set_appender(el, &g_elog_appender_file);
    t1 = _elapsed();
    for (i=0; i<times; ++i) {
        elog_append(el, data, sizeof(data));
    }
    t2 = _elapsed();
    printf("elog times: %d, used time: %d\n", times, (int)(t2-t1));
    elog_free(el);

}

void
test_elog4(int times) {
    uint64_t t1, t2; 
    char data[1024];
    //long sz;
    int i;
    for (i=0; i<sizeof(data); ++i) {
        data[i] = '0';
    }
    data[sizeof(data)-1] = '\0';

    struct elog* el = elog_create("/home/lvxiaojun/log/testlog.log");
    elog_set_appender(el, &g_elog_appender_rollfile);
    struct elog_rollfile_conf conf;
    conf.file_max_num = 2;
    conf.file_max_size = 1024*1024*100;
    elog_appender_rollfile_config(el, &conf);

    t1 = _elapsed();
    for (i=0; i<times; ++i) {
        elog_append(el, data, sizeof(data));
    }
    t2 = _elapsed();
    printf("elog times: %d, used time: %d\n", times, (int)(t2-t1));
    elog_free(el);

}

void
test_log(int times) {
    uint64_t t1, t2; 
    char data[1024];
    long sz;
    int i;
    for (i=0; i<sizeof(data); ++i) {
        data[i] = '0';
    }
    data[sizeof(data)-1] = '\0';
    FILE* fp = fopen("/home/lvxiaojun/log/test1.log", "w+");
    setbuf(fp, NULL);

    FILE* fp2 = fopen("/home/lvxiaojun/log/test2.log", "w+");
    setbuf(fp2, NULL);

    t1 = _elapsed();
    for (i=0; i<times; ++i) {
        fprintf(fp, data);
    }
    t2 = _elapsed();
    sz = ftell(fp);
    fclose(fp);
    printf("write size: %ld, times: %d, fprintf used time: %d\n", sz, times, (int)(t2-t1));
/*
    t1 = _elapsed();
    for (i=0; i<times; ++i) {
        fwrite(data, sizeof(data)-1, 1, fp2);
    }
    t2 = _elapsed();
    sz = ftell(fp2);
    fclose(fp2);
    printf("write size: %ld, times: %d, fwrite used time: %d\n", sz, times, (int)(t2-t1));
*/
}

# ifdef __GNUC__
#  define PH_GCC_VERSION (__GNUC__ * 10000 \
            + __GNUC_MINOR__ * 100 \
            + __GNUC_PATCHLEVEL__)
# else
#  define PH_GCC_VERSION 0
#endif

# if PH_GCC_VERSION >= 40600
#  define ph_static_assert(expr, msg) #_Static_assert(expr, #msg)
# else
#  define ph_static_assert(expr, msg) \
    typedef struct { \
        int static_assertion_failed_##msg : !!(expr); \
    } static_assertion_failed##__LINE__;
#endif

struct sc_library_init_entry {
    int prio;
    void (*init)();
    void (*fini)();
};

struct sc_library {
    struct sc_library_init_entry* p;
    int cap;
    int sz;
};

static struct sc_library* L = NULL;
void
sc_library_init_entry_register(struct sc_library_init_entry* entry) {
    if (L == NULL) {
        L = malloc(sizeof(*L));
        memset(L, 0, sizeof(*L));
    }
    if (L->sz >= L->cap) {
        L->cap *= 2;
        if (L->cap == 0)
            L->cap = 1;
        L->p = realloc(L->p, sizeof(struct sc_library_init_entry) * L->cap);
    }
    L->p[L->sz] = *entry;
    L->sz++;
}

#define SC_LIBRARY_INIT(initfn, finifn, prio) \
    __attribute__((constructor)) \
    void _sc_library_init_##initfn() { \
        printf("%s\n", __FUNCTION__); \
        struct sc_library_init_entry entry = { prio, initfn, finifn }; \
        sc_library_init_entry_register(&entry); \
    }

static int _compare_library_init_entry(const void* p1, const void* p2) {
    const struct sc_library_init_entry* e1 = p1;
    const struct sc_library_init_entry* e2 = p2;
    return e1->prio - e2->prio;
}

void
sc_library_init() {
    qsort(L->p, L->sz, sizeof(L->p[0]), _compare_library_init_entry);
    if (L == NULL)
        return;
    struct sc_library_init_entry* entry;
    int i;
    for (i=0; i<L->sz; ++i) {
        entry = &L->p[i];
        if (entry->init) {
            entry->init();
        }
    }
}

void
sc_library_fini() {
    if (L == NULL)
        return;
    struct sc_library_init_entry* entry;
    int i;
    for (i=L->sz-1; i>=0; --i) {
        entry = &L->p[i];
        if (entry->fini) {
            entry->fini();
        }
    }
    free(L->p);
    free(L);
}

static void init1() {
    printf("init1\n");
}
static void fini1() {
    printf("fini1\n");
}
static void init2() {
    printf("init2\n");
}
static void fini2() {
    printf("fini2\n");
}
SC_LIBRARY_INIT(init1, fini1, 100)
SC_LIBRARY_INIT(init2, fini2, 101)

void
test_copy(int times) {
    char src[4*1024];
    char dest[4*1024];
    int i;
    uint64_t t1,t2;
    t1 = _elapsed();
    for (i=0; i<times; ++i) {
        memcpy(dest, src, sizeof(src));
    }
    t2 = _elapsed();
    printf("memcpy use time: %d\n", (int)(t2-t1));
    
    t1 = _elapsed();
    for (i=0; i<times; ++i) {
        memmove(dest, src, sizeof(src));
    }
    t2 = _elapsed();
    printf("memmove use time: %d\n", (int)(t2-t1));
}

void
dump_str(const char* str) {
    //printf("dump_str: %s\n", str);
    while (*str) {
        printf("\\0x%0x", *str);
        str++;
    }
    printf("\n");
}

void
_encode(const uint8_t* bytes, int nbyte) {
    char str[sc_bytestr_encode_leastn(nbyte)];
    sc_bytestr_encode(bytes, nbyte, str, sizeof(str));
    dump_str(str);
    uint8_t byt[nbyte];
    sc_bytestr_decode(str, sizeof(str)-1, byt, sizeof(byt));
    int i;
    for (i=0; i<nbyte; ++i) {
        printf("%d ", byt[i]);
    }
    printf("\n");
    assert(memcmp(bytes, byt, sizeof(byt)) == 0);
}

void
test_encode() {
    uint8_t bytes[256];
    int i;
    for (i=0; i<sizeof(bytes); ++i) {
        bytes[i] = i;
        _encode(bytes, i+1);
    }    
}

int 
main(int argc, char* argv[]) {
    int times = 1;
    if (argc > 1)
        times = strtol(argv[1], NULL, 10);
   
    //int32_t r = sc_cstr_to_int32("RES");
    //printf("r = %d\n", r);
    //int ret = sc_cstr_compare_int32("RES", r);
    //printf("ret = %d\n", ret);
    //printf("%d\n",  memcmp(&r, "RES", 3));
    //printf("%c\n","RES"[0]);
    //sc_library_init();
    //sc_library_fini();
    //ph_static_assert(sizeof(int)==1, intsize_must4);
    //test_lur();
    //test_args();
    //test_freeid();
    //test_hashid();
    //test_gfreeid();
    //test_redis();
    //test_freelist();
    //test_map();
    //test_elog2();
    //test_log(times);
    //test_elog4(times);
    //test_redisnew(times);
    //test_copy(times);
    test_encode();
    return 0;
}
