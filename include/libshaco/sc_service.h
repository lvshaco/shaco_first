#ifndef __sc_service_h__
#define __sc_service_h__

#include <stdint.h>
#include <stdbool.h>
#include "dlmodule.h"

#define SERVICE_INVALID -1
#define SERVICE_HOST 0xff

#define SERVICE_SELF ((s)->dl.content)
#define SERVICE_NAME ((s)->dl.name)
#define SERVICE_ID ((s)->serviceid)
#define ST_SOCK 0
#define ST_SERV 1

union sc_service_result {
    int32_t  i32;
    uint32_t n32;
    int64_t  i64;
    uint64_t n64;
    void     *p;
};

struct sc_service_arg {
    int type;
    int source;
    int dest;
    const void *msg;
    size_t sz;
    struct sc_service_result *res;
};

/*
struct service_message {
    uint32_t sessionid;
    int32_t source;
    int32_t type;
    int32_t sz;
    void* msg;
    void* result;
    void* p1;
    void* p2;
    void* p3;
    int32_t i1;
    int32_t i2;
    int32_t i3;
    int64_t n1;
};
*/

struct sc_service {
    int serviceid; // >= 0, will not change since loaded
    bool inited;
    struct dlmodule dl;
};

int sc_service_load(const char* name);
int sc_service_prepare(const char* name);
int sc_service_reload(const char* name);
int sc_service_reload_byid(int serviceid);
int sc_service_query_id(const char* name);
const char* sc_service_query_name(int serviceid);

int sc_service_run(int handle, struct sc_service_arg *arg);

/*
int service_notify_service(int serviceid, struct service_message* sm);
int service_notify_net(int serviceid, struct net_message* nm);
int service_notify_time(int serviceid);
int service_notify_nodemsg(int serviceid, int id, void* msg, int sz);
int service_notify_usermsg(int serviceid, int id, void* msg, int sz);
*/

//int sc_handler(const char* name, int* handler);
#endif
