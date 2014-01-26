#ifndef __redis_h__
#define __redis_h__

#include "sh_util.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#define DEPTH 7

#define REDIS_REPLY_UNDO 0
#define REDIS_REPLY_STRING 1
#define REDIS_REPLY_ARRAY 2
#define REDIS_REPLY_INTEGER 3
#define REDIS_REPLY_NIL 4
#define REDIS_REPLY_STATUS 5
#define REDIS_REPLY_ERROR 6

#define REDIS_ERROR 0
#define REDIS_SUCCEED 1
#define REDIS_NEXTTIME 2

#define REDIS_REPLYBUF(reply) ((reply)->reader.buf+(reply)->reader.sz)
#define REDIS_REPLYSPACE(reply) ((reply)->reader.cap - (reply)->reader.sz)
#define REDIS_ITEM(reply) ((reply)->stack[0])

union redis_value {
    uint64_t u;
    int64_t i;
    struct {
        char* p;
        int len;
    };
};

struct redis_replyitem {
    int type; // see REDIS_REPLY_*
    union redis_value value;
    int nchild;
    struct redis_replyitem* child;
};

struct redis_replyitempool {
    int n;
    int index;
    struct redis_replyitem* p;
};

struct redis_reader {
    int cap;
    int sz; 
    int pos;
    int pos_last;
    char* buf;
    bool my;
};

struct redis_reply {
    struct redis_reader reader; 
    struct redis_replyitempool pool;
    struct redis_replyitem* stack[DEPTH];
    int level;
    int result; // see REDIS_*
};

int  redis_getreply(struct redis_reply* reply);
int  redis_initreply(struct redis_reply* reply, int max, int bufcap);
void redis_finireply(struct redis_reply* reply);
void redis_resetreply(struct redis_reply* reply);
void redis_resetreplybuf(struct redis_reply* reply, char* buf, int cap);

void redis_walkreply(struct redis_reply* reply);
int  redis_command(char *buf, int sz, const char *cmd, const char *fmt, ...);
char *redis_formatcommand(const char *fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
;

char *redis_formatcommand2(char *cmd, int sz, const char *fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf, 3, 4)))
#endif
;

char *redis_formatcommand3(char *cmd, int sz, const char *fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf, 3, 4)))
#endif
;

static inline int
redis_bulkitem_isnull(struct redis_replyitem* item) {
    return item->value.len <= 0;
}

static inline uint32_t
redis_bulkitem_toul(struct redis_replyitem* item) {
    if (redis_bulkitem_isnull(item))
        return 0;
    char tmp[16];
    int l = min(sizeof(tmp)-1, item->value.len);
    memcpy(tmp, item->value.p, l);
    tmp[l] = '\0';
    return strtoul(tmp, NULL, 10); 
}

static inline int64_t
redis_to_int64(struct redis_replyitem* item) {
    if (redis_bulkitem_isnull(item))
        return 0;
    char tmp[32];
    int l = min(sizeof(tmp)-1, item->value.len);
    memcpy(tmp, item->value.p, l);
    tmp[l] = '\0';
    return strtold(tmp, NULL);
}

static inline bool
redis_to_status(struct redis_replyitem* item) {
    return item->type == REDIS_REPLY_STATUS;
}

static inline int
redis_to_string(struct redis_replyitem *item, char* str, int sz) {
    if (sz <= 0)
        return 0;
    if (item->type == REDIS_REPLY_STRING ||
        item->type == REDIS_REPLY_STATUS ||
        item->type == REDIS_REPLY_ERROR) {
        int len = item->value.len;
        if (len > 0) {
            if (len >= sz)
                len = sz - 1;
            memcpy(str, item->value.p, len);
            str[len] = '\0';
            return len;
        }
    }
    str[0] = '\0';
    return 0;
}

#endif
