#ifndef __dlmodule_h__
#define __dlmodule_h__

struct service;
struct service_message;
struct net_message;

struct dlmodule {
    char* name;
    void* content;
    void* handle;
    void* (*create)();
    void  (*free)(void* pointer);
    int   (*init)(struct service* s);
    int   (*reload)(struct service* s);
    void  (*service)(struct service* s, struct service_message* sm);
    void  (*time)(struct service* s);
    void  (*net)(struct service* s, struct net_message* nm);
    void  (*usermsg)(struct service* s, int id, void* msg, int sz);
};

int dlmodule_load(struct dlmodule* dl, const char* name);
void dlmodule_close(struct dlmodule* dl);
int dlmodule_reload(struct dlmodule* dl);

#endif
