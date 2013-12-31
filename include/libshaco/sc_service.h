#ifndef __sc_service_h__
#define __sc_service_h__

#include <stdint.h>
#include <stdbool.h>
#include "dlmodule.h"

#define SERVICE_INVALID -1

#define SERVICE_SELF ((s)->dl.content)
#define SERVICE_NAME ((s)->dl.name)
#define SERVICE_ID ((s)->serviceid)

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

int service_main(int serviceid, int session, int source, const void *msg, int sz);
int service_time(int serviceid);
int service_net(int serviceid, struct net_message *nm);

#endif
