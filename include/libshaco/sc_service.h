#ifndef __sc_service_h__
#define __sc_service_h__

#include <stdint.h>
#include <stdbool.h>
#include "dlmodule.h"

#define SERVICE_INVALID -1
#define SERVICE_HOST 0xff

#define SERVICE_SELF ((s)->dl.content)

struct service_message {
    uint32_t sessionid;
    int source;
    int sz;
    void* msg;
};

struct service {
    int serviceid; // >= 0, will not change since loaded
    bool inited;
    struct dlmodule dl;
};

int service_load(const char* name);
int service_prepare(const char* name);
int service_reload(const char* name);
int service_reload_byid(int serviceid);
int service_query_id(const char* name);
const char* service_query_name(int serviceid);

int service_notify_service(int serviceid, struct service_message* sm);
int service_notify_net(int serviceid, struct net_message* nm);
int service_notify_time(int serviceid);
int service_notify_nodemsg(int serviceid, int id, void* msg, int sz);
int service_notify_usermsg(int serviceid, int id, void* msg, int sz);

#endif
