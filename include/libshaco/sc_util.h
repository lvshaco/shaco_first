#ifndef __sc_util_h__
#define __sc_util_h__

#define cs_cstr_to_int32(cstr) ({ \
    int32_t i32 = 0;                   \
    int i;                             \
    for (i=0; i<sizeof(cstr)-1; ++i) { \
        i32 |= ((cstr)[i]) << (i*8);   \
    }                                  \
    i32;                               \
})

#define cs_cstr_compare_int32(cstr, i32) \
    (cs_cstr_to_int32(cstr) == (i32))

#endif
