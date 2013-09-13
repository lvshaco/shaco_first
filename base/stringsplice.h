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

static size_t
stringsplice_create(struct stringsplice* splice, const char* str, char c) {
    size_t n = 0;
    const char* p = str;
    while (*p) {
        char* next = strchrnul(p, c);
        splice->p[n].p = p;
        splice->p[n].len = next - p;
        p = next;
    }
    splice->n = n;
    return n;
}

static const char*
string_new(const char* str, size_t l) {
    char* news = malloc(l+1);
    memcpy(news, str, l);
    news[l] = '\0';
    return news;
}

static size_t
string2array(const char* str, char c, struct array* arr) {
    stringsplice splice;
    stringsplice_create(&splice, str, c);
    if (splice->n == 0) {
        return 0;
    }
    const char* news = NULL;
    int i;
    for (i=0; i<splice->n; ++i) {
        news = string_new(splice.p[i].p, splice.p[i].len);
        array_push(arr, news);
    }
    return splice->n; 
}

static size_t
string2array_st(const char* str, char c, struct array* arr, struct stringtable* st) {
    stringsplice splice;
    stringsplice_create(&splice, str, c);
    if (splice->n == 0) {
        return 0;
    }
    const char* news = NULL;
    int i;
    for (i=0; i<splice->n; ++i) {
        news = stringtable_strl(st, splice.p[i].p, splice.p[i].len);
        array_push(arr, news);
    }
    return splice->n; 
}

#endif
