#include "args.h"
#define _GNU_SOURCE
#include <string.h>

// strchrnul can not found, why?
/*
static char*
_strchrnul(const char* s, int c) {
    while (*s && *s != c) {
        s++;
    }
    return (char*)s;
}
*/

static char*
_strchrskip(char* s, int c) {
    while (*s && *s == c) s++;
    return s;
}

static void
_parse(struct args* A, int max) {
    if (max <= 0)
        max = ARGS_MAX;
    else if (max > ARGS_MAX)
        max = ARGS_MAX;

    int n = 0;
    char* p = A->buf;
    char* next;
    while (*p) {
        p = _strchrskip(p, ' ');
        if (*p == '\0')
            break;
        
        A->argv[n] = p; 
        if (++n >= max)
            break;

        next = strchr(p, ' ');
        if (next == NULL)
            break;
        
        *next = '\0';
        p = next+1;
    }
    A->argc = n;
}

int
args_parsestr(struct args* A, int max, const char* str) {
    strncpy(A->buf, str, ARGS_BUF-1);
    _parse(A, max);
    return A->argc;
}

int
args_parsestrl(struct args* A, int max, const char* str, size_t l) {
    if (l == 0) {
        A->argc = 0;
        return 0;
    }
    if (l >= ARGS_BUF)
        l = ARGS_BUF - 1;
    memcpy(A->buf, str, l);
    A->buf[l] = '\0';
    _parse(A, max);
    return A->argc;
}
