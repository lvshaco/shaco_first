#ifndef __TYPE_H__
#define __TYPE_H__

#include <stddef.h>
#if defined(__GNUC__)
#include <stdint.h>
#define INLINE static inline
#elif defined(WIN32)
#define INLINE __inline
typedef signed char         int8_t;
typedef unsigned char       uint8_t;
typedef signed short        int16_t;
typedef unsigned short      uint16_t;
typedef signed int        int32_t;
typedef unsigned int      uint32_t;
typedef signed __int64      int64_t;
typedef unsigned __int64    uint64_t;
#endif


INLINE void bswap_16 (void* value) {
    uint16_t *ptr = (uint16_t*) value;
    *ptr = ( ((*ptr >> 8) & 0xff) | ((*ptr & 0xff) << 8) );
}

INLINE void bswap_32 (void* value) {
    uint32_t *ptr = (uint32_t*) value;
    *ptr = ( ((*ptr & 0xff000000) >> 24) | ((*ptr & 0x00ff0000) >>  8) |
             ((*ptr & 0x0000ff00) <<  8) | ((*ptr & 0x000000ff) << 24) );
}

INLINE void bswap_64 (void* value) {
    uint64_t *ptr= (uint64_t*)value;
#ifdef WIN32
    *ptr = (
        ((*ptr & 0xff00000000000000Ui64) >> 56) |
        ((*ptr & 0x00ff000000000000Ui64) >> 40) |
        ((*ptr & 0x0000ff0000000000Ui64) >> 24) |
        ((*ptr & 0x000000ff00000000Ui64) >> 8) |
        ((*ptr & 0x00000000ff000000Ui64) << 8) |
        ((*ptr & 0x0000000000ff0000Ui64) << 24) |
        ((*ptr & 0x000000000000ff00Ui64) << 40) |
        ((*ptr & 0x00000000000000ffUi64) << 56)
        );
#else
    *ptr = (
        ((*ptr & 0xff00000000000000ull) >> 56) |
        ((*ptr & 0x00ff000000000000ull) >> 40) |
        ((*ptr & 0x0000ff0000000000ull) >> 24) |
        ((*ptr & 0x000000ff00000000ull) >> 8) |
        ((*ptr & 0x00000000ff000000ull) << 8) |
        ((*ptr & 0x0000000000ff0000ull) << 24) |
        ((*ptr & 0x000000000000ff00ull) << 40) |
        ((*ptr & 0x00000000000000ffull) << 56)
        );
#endif
}

#if defined(WIN32) || ( defined(__GNUC__) && (__BYTE_ORDER==__LITTLE_ENDIAN) )
#define native16(x) bswap_16((void*)x)
#define native32(x) bswap_32((void*)x)
#define native64(x) bswap_64((void*)x)
#else
#define native16(x) (x)
#define native32(x) (x)
#define native64(x) (x)
#endif

#endif
