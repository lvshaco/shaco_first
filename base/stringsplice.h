#ifndef __stringsplice_h__
#define __stringsplice_h__

#include "stringtable.h"
#include "array.h"
#define _GNU_SOURCE
#include <string.h>

#define _STRSPLICE_MAX 128  // 128 is satisfied for most situation

struct _str_splice {
    const char* p;
    size_t len;
};

struct stringsplice {
    size_t n;
    struct _str_splice p[_STRSPLICE_MAX];
};

// strchrnul can not found, why?
static char*
_strchrnul(const char* s, int c) {
    while (*s && *s != c) {
        s++;
    }
    return (char*)s;
}

static size_t
stringsplice_create(struct stringsplice* sp, size_t max, const char* str, char c) {
    if (max > _STRSPLICE_MAX)
        max = _STRSPLICE_MAX;
   
    size_t n = 0; 
    const char* p = str;
    char* next;
    while (*p && n < max) {
        while (*p == c) {
            if (*++p == '\0')
                goto end;
        }
        next = _strchrnul(p, c);
        sp->p[n].p = p;
        sp->p[n].len = next - p;
        n++; 
        if (*next == '\0')
            break;
        p = next+1;
    }
end:
    sp->n = n;
    return n;
}

static const char*
string_new(const char* str, size_t l) {
    char* news = malloc(l+1);
    memcpy(news, str, l);
    news[l] = '\0';
    return news;
}

static inline size_t
string2array(const char* str, char c, struct array* arr) {
    struct stringsplice sp;
    stringsplice_create(&sp, _STRSPLICE_MAX, str, c);
    if (sp.n == 0) {
        return 0;
    }
    const char* news = NULL;
    int i;
    for (i=0; i<sp.n; ++i) {
        news = string_new(sp.p[i].p, sp.p[i].len);
        array_push(arr, (void*)news);
    }
    return sp.n; 
}

static inline size_t
string2array_st(const char* str, char c, struct array* arr, struct stringtable* st) {
    struct stringsplice sp;
    stringsplice_create(&sp, _STRSPLICE_MAX, str, c);
    if (sp.n == 0) {
        return 0;
    }
    const char* news = NULL;
    int i;
    for (i=0; i<sp.n; ++i) {
        news = stringtable_strl(st, sp.p[i].p, sp.p[i].len);
        array_push(arr, (void*)news);
    }
    return sp.n; 
}

#endif
