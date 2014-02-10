#ifndef __dlmodule_h__
#define __dlmodule_h__

struct module;
struct net_message;

struct dlmodule {
    char name[32];
    void* content;
    void* handle;
    void* (*create)();
    void  (*free)(void* pointer);
    int   (*init)(struct module* s);
    void  (*time)(struct module* s);
    void  (*net)(struct module* s, struct net_message* nm);
    void  (*send)(struct module *s, int session, int source, int dest, int type, const void *msg, int sz);
    void  (*main)(struct module *s, int session, int source, int type, const void *msg, int sz);
};

int dlmodule_load(struct dlmodule* dl, const char* name);
void dlmodule_close(struct dlmodule* dl);
int dlmodule_reload(struct dlmodule* dl);

#endif
