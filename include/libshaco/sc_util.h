#ifndef __sc_util_h__
#define __sc_util_h__

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// const cstring to int32, eg: "GMAP"
#define sc_cstr_to_int32(cstr) ({ \
    int32_t i32 = 0;                   \
    int i;                             \
    for (i=0; i<sizeof(cstr)-1; ++i) { \
        i32 |= ((cstr)[i]) << (i*8);   \
    }                                  \
    i32;                               \
})

#define sc_cstr_compare_int32(cstr, i32) \
    (sc_cstr_to_int32(cstr) == (i32))

#define sc_cstr_compare_mem(cstr, mem, sz) \
    (sizeof(cstr) != (sz) || memcmp(cstr, mem, sz))

// countof
#define sc_countof(x) (sizeof(x)/sizeof((x)[0]))

// rand
#define sc_rand(x) rand_r(&(x))

#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif
#ifndef max
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif

static inline void
sc_limitadd(uint32_t add, uint32_t* cur, uint32_t max) {
    uint32_t r = *cur + add;
    *cur = (*cur <= r) ? r : max;
}

// time
#define sc_day_offsecs(tmt) ((tmt).tm_hour * 3600 + (tmt).tm_min * 60 + (tmt).tm_sec)
#define sc_day_base(now, tmnow) ((now) - sc_day_offsecs(tmnow))
#define SC_DAY_SECS (24*3600)

// encode
//#define sc_bytestr_encode_leastn(n) ((n)*8 / 6 + 2)
//#define sc_bytestr_decode_leastn(n) ((n)*6 / 8 + 1)
#define sc_bytestr_encode_leastn(n) ((n)*8 / 7 + 2)
#define sc_bytestr_decode_leastn(n) ((n)*7 / 8 + 1)
int sc_bytestr_encode(const uint8_t* bytes, int nbyte, char* str, int n);
int sc_bytestr_decode(const char* str, int len, uint8_t* bytes, int nbyte);

static inline char *
sc_strncpy(char *dest, const char *src, size_t n) {
    strncpy(dest, src, n);
    if (n > 1) {
        dest[n-1] = '\0';
    }
    return dest;
}

#endif
