#ifndef __util_h__
#define __util_h__

#include <string.h>

static inline int
strncpychk(char* dest, int dlen, const char* src, int slen) {
    if (dlen <= 0 || slen <= 0)
        return 1;
    int len = slen;
    if (len >= dlen)
        len = dlen - 1;
    memcpy(dest, src, len);
    dest[len] = '\0';
    return 0;
}

#endif
