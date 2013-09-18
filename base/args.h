#ifndef __args_h__
#define __args_h__

#include <stdlib.h>

#define ARGS_BUF 1024
#define ARGS_MAX 10

struct args {
    char  buf[ARGS_BUF];
    int   argc;
    char* argv[ARGS_MAX];
};

int args_parsestr(struct args* A, int max, const char* str);
int args_parsestrl(struct args* A, int max, const char* str, size_t l);

#endif
