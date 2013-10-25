#include "redis.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

// todo, the redis-server return error, empty this in release
#define ASSERTD assert

/*
 * redis_replyitem
 */
static inline void
_replyitem_init(struct redis_replyitem* item) {
    memset(item, 0, sizeof(*item));
}

/* 
 * redis_replyitempool
 */
static void
_replyitempool_init(struct redis_replyitempool* pool, int n) {
    assert(n > 0); 
    pool->p = malloc(sizeof(struct redis_replyitem) * n);
    pool->n = n;
    pool->index = 0;
}

static void
_replyitempool_fini(struct redis_replyitempool* pool) { 
    free(pool->p);
    pool->p = NULL;
    pool->n = 0;
    pool->index = 0;
}

static struct redis_replyitem*
_replyitempool_alloc(struct redis_replyitempool* pool, int n) {
    struct redis_replyitem* item = NULL;
    if (pool->index + n <= pool->n) {
        item = &pool->p[pool->index];
        int i;
        for (i=0; i<n; ++i) {
            _replyitem_init(&item[i]);
        }
        pool->index += n;
    }
    return item;
}

static void
_replyitempool_free(struct redis_replyitempool* pool) {
    pool->index = 0;
}

/*
 * redis_reader
 */
#define READ_PTR(reader) ((reader)->buf+(reader)->pos)
static void
_reader_init(struct redis_reader* reader) {
    reader->sz = 0;
    reader->pos = 0;
}

#define _reader_fini _reader_init

/*
 * redis_reply
 */
static inline char*
_readbytes(struct redis_reader* reader, int sz) {
    if (reader->pos + sz <= reader->sz) {
        char* ptr = &reader->buf[reader->pos];
        reader->pos += sz;
        return ptr;
    }
    return NULL;
}

static int
_readline(struct redis_reader* reader) {
    int i;
    for (i=reader->pos; i<reader->sz-1; ++i) {
        if (memcmp(&reader->buf[i], "\r\n", 2) == 0) {
            reader->pos = i+2;
            return 0;
        }
    }
    reader->pos = i;
    return 1;
}

static inline struct redis_replyitem*
_get_visit(struct redis_reply* reply) {
    assert(reply->level < DEPTH);
    return reply->stack[reply->level];
}

static int
_moveto_nextitem(struct redis_reply* reply) {
    for (;;) {
        assert(reply->level < DEPTH);
        reply->level--;
        if (reply->level < 0) {
            return REDIS_SUCCEED;
        }
        reply->stack[reply->level+1] = NULL;
        struct redis_replyitem* parent = reply->stack[reply->level];
        assert(parent);
        assert(parent->type == REDIS_REPLY_ARRAY);
        if (++parent->nchild < parent->value.i) {
            reply->level++;
            reply->stack[reply->level] = &parent->child[parent->nchild];
            //return REDIS_CONTINUE;
            return redis_getreply(reply);
        }
    }
}

static int
_read_header(struct redis_reply* reply) {
    char* ptr = _readbytes(&reply->reader, 1);
    if (ptr == NULL) {
        return REDIS_NEXTTIME;
    }
    struct redis_replyitem* c = _get_visit(reply);
    switch (*ptr) {
    case '+':
        c->type = REDIS_REPLY_STATUS;
        break;
    case '-':
        c->type = REDIS_REPLY_ERROR;
        break;     
    case ':':
        c->type = REDIS_REPLY_INTEGER;
        break;
    case '$':
        c->type = REDIS_REPLY_STRING;
        break;
    case '*':
        c->type = REDIS_REPLY_ARRAY;
        break;
    default:
        ASSERTD(0);
        return REDIS_ERROR;
    }
    struct redis_reader* reader = &reply->reader;
    c->value.p = READ_PTR(reader);
    return redis_getreply(reply);
}

static int
_read_message(struct redis_reply* reply) {
    struct redis_replyitem* item = _get_visit(reply);
    assert(item);
    struct redis_reader* reader = &reply->reader;
    assert(READ_PTR(reader) >= item->value.p);
    if (_readline(reader)) {
        return REDIS_NEXTTIME;
    }
    *(READ_PTR(reader)-2) = '\0';
    item->value.len = READ_PTR(reader) - item->value.p;
    return _moveto_nextitem(reply);
}

static int
_read_integer(struct redis_reply* reply) {
    struct redis_replyitem* item = _get_visit(reply);
    assert(item);
    struct redis_reader* reader = &reply->reader;
    assert(READ_PTR(reader) >= item->value.p);
    if (_readline(reader)) {
        return REDIS_NEXTTIME;
    }
    *(READ_PTR(reader)-2) = '\0';
    char* endptr;
    int64_t i = strtoll(item->value.p, &endptr, 10);
    if (*endptr != '\0') {
        ASSERTD(0);
        return REDIS_ERROR;
    }
    item->value.i = i;
    return _moveto_nextitem(reply);
}

static int
_read_bulkitem(struct redis_reply* reply) {
    struct redis_replyitem* item = _get_visit(reply);
    assert(item);
    struct redis_reader* reader = &reply->reader;
    assert(READ_PTR(reader) >= item->value.p);
    if (item->value.len == 0) {
        if (_readline(reader)) {
            return REDIS_NEXTTIME;
        }
        *(READ_PTR(reader)-2) = '\0';
        char* endptr; 
        int64_t i = strtoll(item->value.p, &endptr, 10);
        if (*endptr != '\0') {
            ASSERTD(0);
            return REDIS_ERROR;
        }
        item->value.len = i;
        item->value.p = READ_PTR(reader);
    }
    if (item->value.len <= 0) {
        item->value.p = "null";
        item->value.len = 4;
    } else { 
        if (_readbytes(reader, item->value.len+2) == NULL) {
            return REDIS_NEXTTIME;
        }
        ASSERTD(memcmp(READ_PTR(reader)-2, "\r\n", 2) == 0);
        *(READ_PTR(reader)-2) = '\0';
    }
    return _moveto_nextitem(reply);
}

static int
_read_multibulkitem(struct redis_reply* reply) {
    struct redis_replyitem* item = _get_visit(reply);
    assert(item);
    struct redis_reader* reader = &reply->reader;
    assert(READ_PTR(reader) >= item->value.p);
    if (_readline(reader)) {
        return REDIS_NEXTTIME;
    }
    *(READ_PTR(reader)-2) = '\0';
    char* endptr;
    int64_t i = strtoll(item->value.p, &endptr, 10);
    if (*endptr != '\0') {
        ASSERTD(0);
        return REDIS_ERROR;
    }
    item->value.i = i;
    if (i <= 0) {
        return _moveto_nextitem(reply);
    } else {
        item->child = _replyitempool_alloc(&reply->pool, i);
        assert(item->child);
        reply->level++;
        reply->stack[reply->level] = &item->child[0];
        return redis_getreply(reply);
    }
}

int
redis_getreply(struct redis_reply* reply) {
    int result = REDIS_ERROR;
    if (reply->level >= DEPTH) {
        //assert(0);
        goto exit; // too depth
    }
    struct redis_replyitem* item = _get_visit(reply);
    assert(item);
    switch (item->type) {
    case REDIS_REPLY_UNDO:
        result = _read_header(reply);
        break;
    case REDIS_REPLY_STATUS:
    case REDIS_REPLY_ERROR:
        result = _read_message(reply);
        break;
    case REDIS_REPLY_INTEGER:
        result = _read_integer(reply);
        break;
    case REDIS_REPLY_STRING:
        result = _read_bulkitem(reply);
        break;
    case REDIS_REPLY_ARRAY:
        result = _read_multibulkitem(reply);
        break;
    default:
        assert(0);
        result = REDIS_ERROR;
        break;
    }
exit:
    reply->result = result;
    return result;
}

int
redis_initreply(struct redis_reply* reply, int max) {
    _reader_init(&reply->reader);
    _replyitempool_init(&reply->pool, max);
    memset(reply->stack, 0, sizeof(reply->stack));
    struct redis_replyitem* root = _replyitempool_alloc(&reply->pool, 1);
    assert(root);
    reply->level = 0;
    reply->stack[0] = root; 
    return 0;
}

void
redis_finireply(struct redis_reply* reply) {
    _reader_fini(&reply->reader);
    _replyitempool_fini(&reply->pool);
}

void
redis_resetreply(struct redis_reply* reply) {
    if (reply->result == REDIS_NEXTTIME) {
        return;
    }
    struct redis_reader* reader = &reply->reader;
    int sz = reader->sz - reader->pos;
    if (sz <= 0 || 
        reply->result == REDIS_ERROR) {
        reader->sz = 0;
    } else if (sz > 0) {
        memmove(reader->buf, READ_PTR(reader), sz);
        reader->sz = sz;
    } 
    reader->pos = 0;
    _replyitempool_free(&reply->pool);
    memset(reply->stack, 0, sizeof(reply->stack));
    struct redis_replyitem* root = _replyitempool_alloc(&reply->pool, 1);
    assert(root);
    reply->level = 0;
    reply->stack[0] = root; 
}

/*
 * dump
 */
static void
_printf_empty(int depth) {
    int i;
    for (i=0; i<depth; ++i) {
        printf("|_|_"); 
    }
}

static void
_walk_reply(struct redis_replyitem* item, int depth) {
    switch (item->type) {
    case REDIS_REPLY_STATUS:
        _printf_empty(depth);
        printf("+%s\n", item->value.p);
        break;
    case REDIS_REPLY_ERROR:
        _printf_empty(depth);
        printf("-%s\n", item->value.p);
        break;
    case REDIS_REPLY_INTEGER:
        _printf_empty(depth);
        printf(":%ld\n", item->value.i);
        break;
    case REDIS_REPLY_STRING:
        _printf_empty(depth);
        printf(" %s\n", item->value.p);
        break;
    case REDIS_REPLY_ARRAY:
        _printf_empty(depth);
        printf("*%ld\n", item->value.i);
        int i;
        for (i=0; i<(int)item->nchild; ++i) {
            struct redis_replyitem* sub = &item->child[i];
            _walk_reply(sub, depth+1);
        }
        break;
    default:
        printf("error occur in depth: %d\n", depth);
        break;
    }
}

void
redis_walkreply(struct redis_reply* reply) {
    _walk_reply(reply->stack[0], 0);
}
