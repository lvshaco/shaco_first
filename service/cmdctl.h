#ifndef __cmdctl_h__
#define __cmdctl_h__

#define CTL_OK 0
#define CTL_NOCOMMAND 1
#define CTL_FAIL 2
#define CTL_ARGLESS 3
#define CTL_ARGINVALID 4
#define CTL_NOSERVICE 5

struct cmdctl;
struct args;
struct memrw;

struct ctl_command {
    const char* name;
    int (*fun)(struct cmdctl* self, struct args* A, struct memrw* rw);
};

#endif
