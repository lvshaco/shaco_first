#ifndef __message_reader_h__
#define __message_reader_h__

#include "message.h"
#include "net.h"

static inline struct UM_BASE*
mread_one(struct mread_buffer* buf, int* e) {
    *e = 0;
    struct UM_BASE* base = buf->ptr;
    int sz = buf->sz;
    int body;
    if (sz >= sizeof(*base)) {
        sz -= sizeof(*base);
        if (base->msgsz >= sizeof(*base)) {
            body = base->msgsz - sizeof(*base);
            if (body > 0) {
                if (body <= sz) {
                    goto ok;
                } else {
                    return NULL;
                }
            } else if (body < 0) {
                return NULL;
            } else {
                goto ok;
            }
        } else {
            *e = NET_ERR_MSG;
            return NULL;
        }
    } else {
        return NULL;
    }
ok:
    buf->ptr += base->msgsz;
    buf->sz  -= base->msgsz;
    return base;
}

static inline struct UM_CLI_BASE*
mread_cli_one(struct mread_buffer* buf, int* e) {
    *e = 0;
    struct UM_CLI_BASE* base = buf->ptr;
    int sz = buf->sz;
    int body;
    uint16_t msgsz;
    if (sz >= sizeof(*base)) {
        if (base->msgsz >= UM_BASE_SZ &&
            base->msgsz <  UM_CLI_MAXSZ) {
            sz -= sizeof(*base);
            msgsz = base->msgsz - UM_CLI_OFF;
            body = msgsz - sizeof(*base);
            if (body > 0) {
                if (body <= sz) {
                    goto ok;
                } else {
                    return NULL;
                }
            } else if (body < 0) {
                return NULL;
            } else {
                goto ok;
            }
        } else {
            *e = NET_ERR_MSG;
            return NULL;
        }
    } else {
        return NULL;
    }
ok:
    buf->ptr += msgsz;
    buf->sz  -= msgsz;
    return base;
}

#endif
