#ifndef __sh_util_h__
#define __sh_util_h__

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

// const cstring to int32, eg: "GMAP"
#define sh_cstr_to_int32(cstr) ({ \
    int32_t i32 = 0;                   \
    int i;                             \
    for (i=0; i<sizeof(cstr)-1; ++i) { \
        i32 |= ((cstr)[i]) << (i*8);   \
    }                                  \
    i32;                               \
})

#define sh_cstr_compare_int32(cstr, i32) \
    (sh_cstr_to_int32(cstr) == (i32))

#define sh_cstr_compare_mem(cstr, mem, sz) \
    (sizeof(cstr) != (sz) || memcmp(cstr, mem, sz))

// countof
#define sh_countof(x) (sizeof(x)/sizeof((x)[0]))

// rand
#define sh_rand(x) rand_r(&(x))
static inline int 
sh_rande(uint32_t *seed) {
    *seed = (*seed) * 1103515245 + 12345;
    return((uint32_t)((*seed)/65536) % 32768);
}

#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif
#ifndef max
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif

static inline void
sh_limitadd(uint32_t add, uint32_t* cur, uint32_t max) {
    uint32_t r = *cur + add;
    *cur = (*cur <= r) ? r : max;
}

// time
#define sh_day_offsecs(tmt) ((tmt).tm_hour * 3600 + (tmt).tm_min * 60 + (tmt).tm_sec)
#define sh_day_base(now, tmnow) ((now) - sh_day_offsecs(tmnow))
#define SC_DAY_SECS (24*3600)

// encode
//#define sh_bytestr_encode_leastn(n) ((n)*8 / 6 + 2)
//#define sh_bytestr_decode_leastn(n) ((n)*6 / 8 + 1)
#define sh_bytestr_encode_leastn(n) ((n)*8 / 7 + 2)
#define sh_bytestr_decode_leastn(n) ((n)*7 / 8 + 1)
int sh_bytestr_encode(const uint8_t* bytes, int nbyte, char* str, int n);
int sh_bytestr_decode(const char* str, int len, uint8_t* bytes, int nbyte);

static inline char *
sh_strncpy(char *dest, const char *src, size_t n) {
    strncpy(dest, src, n);
    if (n > 1) {
        dest[n-1] = '\0';
    }
    return dest;
}

// endian
static inline void
sh_to_bigendian16(uint16_t n, uint8_t *buffer) {
    buffer[0] = (n >> 8) & 0xff;
    buffer[1] = (n) & 0xff;
}

static inline void
sh_to_bigendian32(uint32_t n, uint8_t *buffer) {
    buffer[0] = (n >> 24) & 0xff;
    buffer[1] = (n >> 16) & 0xff;
    buffer[2] = (n >> 8)  & 0xff;
    buffer[3] = (n) & 0xff;
}

static inline void
sh_to_littleendian16(uint16_t n, uint8_t *buffer) {
    buffer[0] = (n) & 0xff;
    buffer[1] = (n >> 8) & 0xff;
}

static inline void
sh_to_littleendian32(uint32_t n, uint8_t *buffer) {
    buffer[0] = (n) & 0xff;
    buffer[1] = (n >> 8) & 0xff;
    buffer[2] = (n >> 16) & 0xff;
    buffer[3] = (n >> 24) & 0xff;
}

static inline uint16_t 
sh_from_bigendian16(const uint8_t *buffer) {
    return buffer[0] << 8 | buffer[1];
}

static inline uint32_t 
sh_from_bigendian32(const uint8_t *buffer) {
    return buffer[0] << 24 | buffer[1] << 16 | buffer[2] << 8 | buffer[3];
}

static inline uint16_t
sh_from_littleendian16(const uint8_t *buffer) {
    return buffer[0] | buffer[1] << 8;
}

static inline uint32_t
sh_from_littleendian32(const uint8_t *buffer) {
    return buffer[0] | buffer[1] << 8 | buffer[2] << 16 | buffer[3] << 24;
}

static int sh_vsnprintf(char *str, size_t size, const char *format, va_list ap);
static int sh_snprintf(char *str, size_t size, const char *format, ...)
#ifdef __GNUC__
__attribute__((format(printf, 3, 4)))
#endif
;

static inline int 
sh_vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    int n = vsnprintf(str, size, format, ap);
    if (n <= 0)
        return 0;
    if (n < (int)size)
        return n;
    return (int)(size-1);
}

static inline int 
sh_snprintf(char *str, size_t size, const char *format, ...) {
    va_list ap;
    int n;
    va_start(ap, format);
    n = sh_vsnprintf(str, size, format, ap);
    va_end(ap);
    return n;
}

#endif
