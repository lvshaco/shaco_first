#ifndef _stringtable_h__
#define _stringtable_h__

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

struct _string {
    struct _string* next;
    uint32_t hash;
    size_t len;
    char str[0];
};

struct stringtable {
    uint32_t size;
    struct _string** slots; 
};

struct stringtable*
stringtable_create(uint32_t hash) {
    struct stringtable* st = malloc(sizeof(*st));
   
    uint32_t size = 1;
    while (size < hash)
        size *= 2;
    st->size = size;
    st->slots = malloc(sizeof(struct _string*) * size);
    return st;
}

void
stringtable_free(struct stringtable* st) {
    if (st == NULL)
        return;

    struct _string* head = NULL;
    struct _string* s = NULL;
    int i;
    for (i=0; i<st->size; ++i) {
        head = st->slots[i];
        while (head) {
            s = head;
            head = s->next;
            free(s);
        }
    }
    free(st->slots);
    free(st);
}

static uint32_t
_hash(const char* str, size_t l) {
    uint32_t h = l;
    size_t step = (l>>5)+1;
    size_t i;
    for (i=l; i>=step; i-=step)
        h = h ^ ((h<<5)+(h>>2)+str[i-1]);
    return h;   
}

static struct _string* 
_string_new(const char* str, size_t l, uint32_t hash) {
    struct _string* s = malloc(sizeof(*s) + l + 1);
    s->next = NULL;
    s->hash = hash;
    s->len = l;
    memcpy(s->str, str, l);
    s->str[l] = '\0';
    return s;
}

const char*
stringtable_strl(struct stringtable* st, const char* str, size_t l) {
    uint32_t h = _hash(str, l);
    uint32_t hmod = h & (st->size-1);
    struct _string* s = st->slots[hmod];
    while (s) {
        if (s->hash == h &&
            s->len == l &&
            memcmp(s->str, str, l) == 0)
            return s->str;
    }
    struct _string* news = _string_new(str, l, h);
    news->next = s;
    st->slots[hmod] = news;
    return news->str;
}

const char*
stringtable_str(struct stringtable* st, const char* str) {
    size_t l = strlen(str);
    return stringtable_strl(st, str, l);
}

#endif
