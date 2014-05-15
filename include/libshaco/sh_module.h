#ifndef __sh_module_h__
#define __sh_module_h__

#include <stdint.h>
#include <stdbool.h>

struct module;
struct net_message;

struct dlmodule {
    char *name;
    void *content;
    void *handle;
    void *(*create)();
    void  (*free)(void* pointer);
    int   (*init)(struct module* s);
    void  (*time)(struct module* s);
    void  (*net) (struct module* s, 
                  struct net_message* nm);
    void  (*send)(struct module *s, 
                  int session, 
                  int source, 
                  int dest, 
                  int type, 
                  const void *msg, 
                  int sz);
    void  (*main)(struct module *s, 
                  int session, 
                  int source, 
                  int type, 
                  const void *msg, 
                  int sz);
};

#define MODULE_INVALID -1
#define MODULE_SELF ((s)->dl.content)
#define MODULE_NAME ((s)->name)
#define MODULE_ID ((s)->moduleid)

struct module {
    int moduleid; // >= 0, will not change since loaded
    bool inited;
    char *name;
    struct dlmodule dl;
};

int module_load(const char* name);
int module_init(const char* name);
int module_reload(const char* name);
int module_reload_byid(int moduleid);
int module_query_id(const char* name);
const char* module_query_module_name(int moduleid);

int module_main(int moduleid, int session, int source, int type, const void *msg, int sz);
int module_time(int moduleid);
int module_net (int moduleid, struct net_message *nm);
int module_send(int moduleid, int session, int source, int dest, int type, const void *msg, int sz);

#endif
