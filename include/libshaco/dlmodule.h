#ifndef __dlmodule_h__
#define __dlmodule_h__

struct service;
struct net_message;

struct dlmodule {
    char name[32];
    void* content;
    void* handle;
    void* (*create)();
    void  (*free)(void* pointer);
    int   (*init)(struct service* s);
    void  (*time)(struct service* s);
    void  (*net)(struct service* s, struct net_message* nm);
    void  (*send)(struct service *s, int session, int source, int dest, int type, const void *msg, int sz);
    void  (*main)(struct service *s, int session, int source, int type, const void *msg, int sz);
};

int dlmodule_load(struct dlmodule* dl, const char* name);
void dlmodule_close(struct dlmodule* dl);
int dlmodule_reload(struct dlmodule* dl);

#endif
