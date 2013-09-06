#ifndef __host_service_h__
#define __host_service_h__

#include <stdint.h>
#include "dlmodule.h"

#define SERVICE_INVALID -1
#define SERVICE_HOST 0xff

#define SERVICE_SELF (s->dl.content)

struct service_message {
    uint32_t sessionid;
    int source;
    int sz;
    void* msg;
};

struct service {
    int serviceid; // >= 0, will not change since loaded
    struct dlmodule dl;
};

int service_init();
void service_fini();

int service_load(const char* name);
int service_reload(const char* name);
int service_query_id(const char* name);
int service_notify_service_message(int destination, struct service_message* sm);
int service_notify_net_message(int destination, struct net_message* nm);
int service_notify_time_message(int destination);

#endif
