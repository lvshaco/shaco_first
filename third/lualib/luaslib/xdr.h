/**
 * @file xdr.h
 * @brief   xdr message
 * @author lvxiaojun
 * @version 
 * @Copyright shengjoy.com
 * @date 2012-10-04
 */
#ifndef __XDR_H__
#define __XDR_H__

#include "type.h"

#define XDR_MT "xdr"

struct xdr;

struct xdr* xdr_create(size_t init_size, size_t max_size);
void xdr_destroy(struct xdr* x);
void xdr_reset(struct xdr* x);

const char* xdr_getbuffer(struct xdr* x, size_t* size);
void xdr_setbuffer(struct xdr* x, const char* buf, size_t size);
uint32_t xdr_getremain(struct xdr* x);

int xdr_packint8(struct xdr* x, int8_t value);
int xdr_packuint8(struct xdr* x, uint8_t value);
int xdr_packint16(struct xdr* x, int16_t value);
int xdr_packuint16(struct xdr* x, uint16_t value);
int xdr_packint32(struct xdr* x, int32_t value);
int xdr_packuint32(struct xdr* x, uint32_t value);
int xdr_packint64(struct xdr* x, int64_t value);
int xdr_packuint64(struct xdr* x, uint64_t value);
int xdr_packfloat(struct xdr* x, float value);
int xdr_packdouble(struct xdr* x, double value);
int xdr_packstring(struct xdr* x, const char* value, uint32_t len);

int xdr_unpackint8(struct xdr* x, int8_t* value);
int xdr_unpackuint8(struct xdr* x, uint8_t* value);
int xdr_unpackint16(struct xdr* x, int16_t* value);
int xdr_unpackuint16(struct xdr* x, uint16_t* value);
int xdr_unpackint32(struct xdr* x, int32_t* value);
int xdr_unpackuint32(struct xdr* x, uint32_t* value);
int xdr_unpackint64(struct xdr* x, int64_t* value);
int xdr_unpackuint64(struct xdr* x, uint64_t* value);
int xdr_unpackfloat(struct xdr* x, float* value);
int xdr_unpackdouble(struct xdr* x, double* value);
int xdr_unpackstring(struct xdr* x, const char** value, uint32_t* len);

#endif

