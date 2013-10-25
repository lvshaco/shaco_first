#ifndef __redis_h__
#define __redis_h__

#include <stdint.h>

#define REDIS_BUF_MAX 16*1024
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
#define REDIS_REPLYSPACE(reply) (REDIS_BUF_MAX - (reply)->reader.sz)

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
    int sz;
    int pos;
    char buf[REDIS_BUF_MAX]; 
};

struct redis_reply {
    struct redis_reader reader; 
    struct redis_replyitempool pool;
    struct redis_replyitem* stack[DEPTH];
    int level;
    int result; // see REDIS_*
};

int  redis_getreply(struct redis_reply* reply);
int  redis_initreply(struct redis_reply* reply, int max);
void redis_finireply(struct redis_reply* reply);
void redis_resetreply(struct redis_reply* reply);
void redis_walkreply(struct redis_reply* reply);

#endif
