#include "redis.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

// todo, the redis-server return error, empty this in release
#define ASSERTD(x) assert(x)

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
_reader_setbuf(struct redis_reader* reader, char* buf, int bufcap) {
    if (reader->my) {
        free(reader->buf);
    }
    reader->sz = 0;
    reader->pos = 0;
    reader->pos_last = 0;
    
    if (bufcap > 0) {
        reader->cap = bufcap;
        if (buf) {
            reader->buf = buf;
            reader->my = false;
        } else {
            reader->buf = malloc(bufcap);
            reader->my = true; 
        }
    } else {
        reader->cap = 0;
        reader->buf = NULL;
        reader->my = false;
    }
}

static void
_reader_init(struct redis_reader* reader, char* buf, int bufcap) {
    reader->my = false;
    _reader_setbuf(reader, buf, bufcap);
}

static void
_reader_fini(struct redis_reader* reader) {
    reader->sz = 0;
    reader->pos = 0;
    reader->pos_last = 0;
    if (reader->my) {
        free(reader->buf);
    }
    reader->buf = NULL;
    reader->cap = 0;
}

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
    //*(READ_PTR(reader)-2) = '\0';
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
    *(READ_PTR(reader)-2) = '\r';
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
        *(READ_PTR(reader)-2) = '\r';
        item->value.len = i;
        item->value.p = READ_PTR(reader);
    }
    int len = item->value.len;
    if (len <= 0) {
        item->value.p = "";
        //item->value.len = 0;
        len = 0;
    }
    if (_readbytes(reader, len+2) == NULL) {
        return REDIS_NEXTTIME;
    }
    ASSERTD(memcmp(READ_PTR(reader)-2, "\r\n", 2) == 0);
    //*(READ_PTR(reader)-2) = '\0';
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
    *(READ_PTR(reader)-2) = '\r';
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
redis_initreply(struct redis_reply* reply, int max, int bufcap) {
    _reader_init(&reply->reader, NULL, bufcap);
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
    struct redis_reader* reader = &reply->reader;
    if (reader->pos == 0) {
        return;
    }
    switch (reply->result) {
    case REDIS_ERROR:
        reader->sz = 0;
        reader->pos = 0;
        reader->pos_last = 0;
        break;
    case REDIS_SUCCEED:
        assert(reader->pos <= reader->sz);
        if (reader->pos == reader->sz) {
            reader->sz = 0;
            reader->pos = 0;
            reader->pos_last = 0;
        } else {
            reader->pos_last = reader->pos;
        }
        break;
    case REDIS_NEXTTIME:
        // if pos_last == 0, then reader buf is no enough for one reply
        assert(reader->pos_last > 0);
        assert(reader->pos_last <= reader->sz);
        reader->sz = reader->sz - reader->pos_last;
        if (reader->sz > 0) {
            memmove(reader->buf, 
                    reader->buf + reader->pos_last, 
                    reader->sz);
        }
        reader->pos = 0;
        reader->pos_last = 0;
        break;
    }
    _replyitempool_free(&reply->pool);
    memset(reply->stack, 0, sizeof(reply->stack));
    struct redis_replyitem* root = _replyitempool_alloc(&reply->pool, 1);
    assert(root);
    reply->level = 0;
    reply->stack[0] = root; 
}

void 
redis_resetreplybuf(struct redis_reply* reply, char* buf, int sz) {
    struct redis_reader* reader = &reply->reader;
    _reader_setbuf(reader, buf, sz);
    reader->sz  = sz;

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
_print_empty(int depth) {
    int i;
    for (i=0; i<depth; ++i) {
        printf("|_|_"); 
    }
}

static void
_print_type(char type, int depth) {
    _print_empty(depth);
    printf("%c", type);
}

static void
_print_stringitem(struct redis_replyitem* item) {
    int len = item->value.len;
    if (len <= 0) {
        printf("null\n");
    } else {
        char tmp[1024];
        if (len >= sizeof(tmp))
            len = sizeof(tmp)-1;
        strncpy(tmp, item->value.p, len);
        tmp[len] = '\0';
        printf("%s\n", tmp);
    }
}

static void
_walk_reply(struct redis_replyitem* item, int depth) {
    switch (item->type) {
    case REDIS_REPLY_STATUS:
        _print_type('+', depth);
        _print_stringitem(item);
        break;
    case REDIS_REPLY_ERROR:
        _print_type('-', depth);
        _print_stringitem(item);
        break;
    case REDIS_REPLY_INTEGER:
        _print_type(':', depth);
        printf("%lld\n", (long long int)item->value.i);
        break;
    case REDIS_REPLY_STRING:
        _print_type(' ', depth);
        _print_stringitem(item);
        break;
    case REDIS_REPLY_ARRAY:
        _print_type('*', depth);
        printf("%lld\n", (long long int)item->value.i);
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

static int 
itoa(int v, char *p) {
    int n = 0;
    bool plus = true;
    if (v<0) {
        p[n] = '-';
        v = -v;
        n++;
        plus = false;
    }
    do {
        int c = v%10;
        v /= 10;
        p[n] = c + '0';
        n++;
    } while (v);
    p[n] = '\0';

    int i;
    if (plus) {
        for (i=0; i<n/2; ++i) {
            char t = p[i];
            p[i] = p[n-i-1];
            p[n-i-1] = t;

        }
    } else {
        for (i=1; i<(n+1)/2; ++i) {
            char t = p[i];
            p[i] = p[n-i];
            p[n-i] = t;
        }
    }
    return n;
}

static int
ntoa(unsigned int v, char *p) {
    int n = 0;
    do {
        unsigned int c = v%10;
        v /= 10;
        p[n] = c + '0';
        n++;
    } while (v);
    p[n] = '\0';

    int i;
    for (i=0; i<n/2; ++i) {
        char t = p[i];
        p[i] = p[n-i-1];
        p[n-i-1] = t;

    }
    return n;
}

int
redis_format(char **buf, int sz, const char *fmt, ...) {
    assert(buf);
    struct bytes {
        char *p;
        int sz;
    };
    int argsz = 4;
    int argc = 0;
    int total = 0;
    struct bytes *argv = malloc(sizeof(struct bytes) * argsz); 
    argv[0].p = NULL;
    argv[0].sz = 0;
  
    int l;
    const char *s;
    char tmp[24];
    const char *newarg = NULL;
    struct bytes *arg;
    
    va_list ap;
    va_start(ap, fmt);
     
    while (*fmt) {
        if (*fmt != ' ') {
            if (!newarg) {
                newarg = fmt;
                if (argc == argsz) {
                    argsz *= 2;
                    argv = realloc(argv, sizeof(struct bytes) * argsz);
                }
                argv[argc].p = NULL;
                argv[argc].sz = 0;
                argc++;
            }
            if (*fmt == '%') {
                if (fmt > newarg) {
                    l = fmt - newarg;
                    arg = &argv[argc-1]; 
                    arg->p = realloc(arg->p, arg->sz+l);
                    memcpy(arg->p + arg->sz, newarg, l);
                    arg->sz += l;

                    total += l;
                }
                switch (*(++fmt)) {
                case 's': {
                    s = va_arg(ap, const char*);
                    l = strlen(s); 
                    }
                    break;
                case 'b':
                    s = va_arg(ap, const char*);
                    l = va_arg(ap, size_t);
                    break;
                case 'd':
                    s = tmp;
                    l = itoa(va_arg(ap, int), tmp);  
                    break;
                case 'u':
                    s = tmp;
                    l = ntoa(va_arg(ap, unsigned int), tmp);
                    break;
                case 'U':
                    s = tmp;
                    l = snprintf(tmp, sizeof(tmp), "%llu", va_arg(ap, unsigned long long));
                    break;
                case 'f':
                    s = tmp;
                    l = snprintf(tmp, sizeof(tmp), "%.3f", va_arg(ap, double));
                    break;
                case '%':
                    s = "%";
                    l = 1;
                    break;
                default:
                    goto err;
                }
                if (l > 0) {
                    arg = &argv[argc-1]; 
                    arg->p = realloc(arg->p, arg->sz+l);
                    memcpy(arg->p + arg->sz, s, l);
                    arg->sz += l;

                    total += l;
                }

                newarg = fmt+1;
            }
        } else {
            if (newarg) {
                l = fmt - newarg;
                if (l) {
                    arg = &argv[argc-1];
                    arg->p = realloc(arg->p, arg->sz+l);
                    memcpy(arg->p + arg->sz, newarg, l);
                    arg->sz += l;
                    
                    total += l;
                }
                newarg = NULL;
            }
        }
        fmt++;
    }
    if (newarg) {
        l = fmt - newarg;
        if (l) {
            arg = &argv[argc-1];
            arg->p = realloc(arg->p, arg->sz+l);
            memcpy(arg->p + arg->sz, newarg, l);
            arg->sz += l;
            
            total += l;
        }
        newarg = NULL;
    }
    va_end(ap);

    if (argc == 0) {
        goto err;
    }
    
    int msz = total+1+13+15*argc;
    if (*buf == NULL) {
        *buf = malloc(msz);
    } else {
        if (sz < msz)
            goto err;
    }
    char *p = *buf;
    
    *p++ = '*';
    p += itoa(argc, p);
    *p++ = '\r';
    *p++ = '\n';
    
    int i;
    for (i=0; i<argc; ++i) {
        arg = &argv[i];

        *p++ = '$';
        p += itoa(arg->sz, p);
        *p++ = '\r';
        *p++ = '\n';

        if (arg->p) {
            memcpy(p, arg->p, arg->sz);
            p += arg->sz;
        }
        *p++ = '\r';
        *p++ = '\n';
        
        free(arg->p);
    }
    *p = '\0';
    free(argv);
    return p - *buf;
err:
    for (i=0; i<argc; ++i) {
        free(argv[i].p);
    }
    free(argv);
    return 0;
}
