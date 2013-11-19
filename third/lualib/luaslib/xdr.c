/**
 * @file xdr.c
 * @brief   xdr message
 * @author lvxiaojun
 * @version 
 * @Copyright shengjoy.com
 * @date 2012-10-04
 */
#include "xdr.h"
#include <stdlib.h>
#include <string.h>

#define XDR_BUF_DEFAULT_SIZE 64

/*****************************************************************************/
// xdr buffer
struct xdr_buf {
    size_t size;
    char data[0];
};

static struct xdr_buf* xdr_buf_create(size_t init_size) {
    size_t size = init_size == 0 ? XDR_BUF_DEFAULT_SIZE : init_size;
    struct xdr_buf* buf = (struct xdr_buf*)malloc(sizeof(struct xdr_buf) + size);
    buf->size = size;
    return buf;
}

static void xdr_buf_destroy(struct xdr_buf* buf) {
    free(buf);
}

static void xdr_buf_expand(struct xdr_buf** buf, size_t size) {
    size_t new_size = (*buf)->size + size;
    *buf = (struct xdr_buf*)realloc(*buf, sizeof(struct xdr_buf) + new_size);
    (*buf)->size = new_size;
}

/*****************************************************************************/
// xdr
struct xdr {
    struct xdr_buf* buf;
    size_t max_size;
    char* pos;
};

struct xdr* xdr_create(size_t init_size, size_t max_size) {
    struct xdr* x = (struct xdr*)malloc(sizeof(struct xdr));
    x->buf = xdr_buf_create(init_size);
    x->pos = x->buf->data;
    x->max_size = max_size > x->buf->size ? max_size : x->buf->size;
    return x;
}

void xdr_destroy(struct xdr* x) {
    if (x->buf)
        xdr_buf_destroy(x->buf);
    free(x);
}

void xdr_reset(struct xdr* x) {
    if (x->buf)
        x->pos = x->buf->data;
}

const char* xdr_getbuffer(struct xdr* x, size_t* size) {
    if (x->buf == NULL) return NULL;
    *size = x->pos - x->buf->data;
    return x->buf->data;
}

void xdr_setbuffer(struct xdr* x, const char* buf, size_t size) {
    if (x->buf == NULL) {
        x->buf = xdr_buf_create(size);
    } else if (x->buf->size < size) {
        xdr_buf_destroy(x->buf);
        x->buf = xdr_buf_create(size);
    }
    memcpy(x->buf->data, buf, size);
    x->buf->size = size;
    x->pos = x->buf->data;
}

uint32_t xdr_getremain(struct xdr* x) {
    char* end = x->buf->data + x->buf->size;
    return (end > x->pos) ? (uint32_t)(end - x->pos) : 0;
}

/*****************************************************************************/
static int _xdr_check_pack(struct xdr* x, size_t size) {
    size_t expand_size;
    int32_t diff;
    if (x->pos + size <= x->buf->data + x->buf->size) 
        return 0;
    if (x->max_size == x->buf->size)
        return -1;
    expand_size = x->buf->size;
    if (expand_size + x->buf->size > x->max_size)
        expand_size = x->max_size - x->buf->size;
    diff = x->pos - x->buf->data;
    xdr_buf_expand(&x->buf, expand_size);
    x->pos = x->buf->data + diff;
    return 0;
}

int xdr_packint8(struct xdr* x, int8_t value) {
    if (_xdr_check_pack(x, sizeof(value)) != 0) return -1;
    *(int8_t*)x->pos = value;
    x->pos += sizeof(value);
    return 0;
}

int xdr_packuint8(struct xdr* x, uint8_t value) {
    if (_xdr_check_pack(x, sizeof(value)) != 0) return -1;
    *(uint8_t*)x->pos = value;
    x->pos += sizeof(value);
    return 0;
}

int xdr_packint16(struct xdr* x, int16_t value) {
    if (_xdr_check_pack(x, sizeof(value)) != 0) return -1;
    native16(&value);
    *(int16_t*)x->pos = value;
    x->pos += sizeof(value);
    return 0;
}

int xdr_packuint16(struct xdr* x, uint16_t value) {
    if (_xdr_check_pack(x, sizeof(value)) != 0) return -1;
    native16(&value);
    *(uint16_t*)x->pos = value;
    x->pos += sizeof(value);
    return 0;
}

int xdr_packint32(struct xdr* x, int32_t value) {
    if (_xdr_check_pack(x, sizeof(value)) != 0) return -1;
    native32(&value);
    *(int32_t*)x->pos = value;
    x->pos += sizeof(value);
    return 0;
}

int xdr_packuint32(struct xdr* x, uint32_t value) {
    if (_xdr_check_pack(x, sizeof(value)) != 0) return -1;
    native32(&value);
    *(uint32_t*)x->pos = value;
    x->pos += sizeof(value);
    return 0;
}

int xdr_packint64(struct xdr* x, int64_t value) {
    if (_xdr_check_pack(x, sizeof(value)) != 0) return -1;
    native64(&value);
    *(int64_t*)x->pos = value;
    x->pos += sizeof(value);
    return 0;
}

int xdr_packuint64(struct xdr* x, uint64_t value) {
    if (_xdr_check_pack(x, sizeof(value)) != 0) return -1;
    native64(&value);
    *(uint64_t*)x->pos = value;
    x->pos += sizeof(value);
    return 0;
}

int xdr_packfloat(struct xdr* x, float value) {
    if (_xdr_check_pack(x, sizeof(value)) != 0) return -1;
    native32(&value);
    *(float*)x->pos = value;
    x->pos += sizeof(value);
    return 0;
}

int xdr_packdouble(struct xdr* x, double value) {
    if (_xdr_check_pack(x, sizeof(value)) != 0) return -1;
    native64(&value);
    *(double*)x->pos = value;
    x->pos += sizeof(value);
    return 0;
}

int xdr_packstring(struct xdr* x, const char* value, uint32_t len) {
    uint32_t l;
    if (_xdr_check_pack(x, len + sizeof(len)) != 0) return -1;
    l = len;
    native32(&l);
    *(uint32_t*)x->pos = l;
    x->pos += sizeof(len);
    memcpy(x->pos, value, len);
    x->pos += len;
    return 0;
}
  
/*****************************************************************************/
static int _xdr_check_unpack(struct xdr* x, size_t size) {
    return (x->pos + size <= x->buf->data + x->buf->size) ? 0 : -1;
}

int xdr_unpackint8(struct xdr* x, int8_t* value) {
    if (_xdr_check_unpack(x, sizeof(*value)) != 0) return -1;
    *value = *(int8_t*)x->pos;
    x->pos += sizeof(*value);
    return 0;
}

int xdr_unpackuint8(struct xdr* x, uint8_t* value) {
    if (_xdr_check_unpack(x, sizeof(*value)) != 0) return -1;
    *value = *(uint8_t*)x->pos;
    x->pos += sizeof(*value);
    return 0;
}

int xdr_unpackint16(struct xdr* x, int16_t* value) {
    if (_xdr_check_unpack(x, sizeof(*value)) != 0) return -1;
    *value = *(int16_t*)x->pos;
    native16(value);
    x->pos += sizeof(*value);
    return 0;
}

int xdr_unpackuint16(struct xdr* x, uint16_t* value) {
    if (_xdr_check_unpack(x, sizeof(*value)) != 0) return -1;
    *value = *(uint16_t*)x->pos;
    native16(value);
    x->pos += sizeof(*value);
    return 0;
}

int xdr_unpackint32(struct xdr* x, int32_t* value) {
    if (_xdr_check_unpack(x, sizeof(*value)) != 0) return -1;
    *value = *(int32_t*)x->pos;
    native32(value);
    x->pos += sizeof(*value);
    return 0;
}

int xdr_unpackuint32(struct xdr* x, uint32_t* value) {
    if (_xdr_check_unpack(x, sizeof(*value)) != 0) return -1;
    *value = *(uint32_t*)x->pos;
    native32(value);
    x->pos += sizeof(*value);
    return 0;
}

int xdr_unpackint64(struct xdr* x, int64_t* value) {
    if (_xdr_check_unpack(x, sizeof(*value)) != 0) return -1;
    *value = *(int64_t*)x->pos;
    native64(value);
    x->pos += sizeof(*value);
    return 0;
}

int xdr_unpackuint64(struct xdr* x, uint64_t* value) {
    if (_xdr_check_unpack(x, sizeof(*value)) != 0) return -1;
    *value = *(uint64_t*)x->pos;
    native64(value);
    x->pos += sizeof(*value);
    return 0;
}

int xdr_unpackfloat(struct xdr* x, float* value) {
    if (_xdr_check_unpack(x, sizeof(*value)) != 0) return -1;
    *value = *(float*)x->pos;
    native32(value);
    x->pos += sizeof(*value);
    return 0;
}

int xdr_unpackdouble(struct xdr* x, double* value) {
    if (_xdr_check_unpack(x, sizeof(*value)) != 0) return -1;
    *value = *(double*)x->pos;
    native64(value);
    x->pos += sizeof(*value);
    return 0;
}

int xdr_unpackstring(struct xdr* x, const char** value, uint32_t* len) {
    if (_xdr_check_unpack(x, sizeof(*len)) != 0) return -1;
    *len = *(uint32_t*)x->pos;
    native32(len);
    x->pos += sizeof(*len);
    if (_xdr_check_unpack(x, *len) != 0) return -1;
    *value = (const char*)x->pos;
    x->pos += *len;
    return 0;
}

