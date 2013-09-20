#include "host_service.h"
#include "host_node.h"
#include "host_net.h"
#include "host_log.h"
#include "host.h"
#include "node_type.h"
#include "user_message.h"
#include <stdlib.h>
#include <string.h>

static int
_connect_center(struct service* s) {
    const char* addr = host_getstr("center_ip", "");
    int port = host_getint("center_port", 0);
    host_info("connect to %s:%u ...", addr, port);
    if (host_net_connect(addr, port, true, s->serviceid, 0) < 0) { 
        return 1;
    }
    return 0;
}

int
centerc_init(struct service* s) {
    if (_connect_center(s))
        return 1;
    return 0;
}

static void
_reg_request(int id) {
    struct host_node* me = host_me();
    UM_DEFFIX(UM_node_reg, reg, UMID_NODE_REG);
    reg.addr = me->addr;
    reg.port = me->port;
    UM_SEND(id, &reg, sizeof(reg));
}

static void
_sub_request(int id) {
    const char* str;
    str = host_getstr("node_sub", "");
    if (str[0] == '\0') {
        return;
    }
    char tmp[(NODE_TYPE_MAX+1) * HNODE_NAME_MAX];
    strncpy(tmp, str, sizeof(tmp)-1);
   
    UM_DEFVAR(UM_node_subs, req, UMID_NODE_SUB);
    const char* p = tmp;
    char* next;
    int n = 0;
    int tid;
    while (p) {
        next = strchr(p, ',');
        if (next)
            *next = '\0';
        tid = host_node_typeid(p);
        if (tid == -1) {
            host_warning("subscribe skip unknown node:%s", p);
        } else {
            req->subs[n++] = tid; 
        }
        if (next == NULL)
            break;
        p = next+1;
    }
    req->n = n; 
    UM_SEND(id, req, UM_node_subs_size(req));
}

void
centerc_service(struct service* s, struct service_message* sm) {
    int center_connid = sm->sessionid;
    _sub_request(center_connid);
}

void
centerc_net(struct service* s, struct net_message* nm) {
    switch (nm->type) {
    case NETE_CONNECT:
        host_info("connect to center ok");
        host_net_subscribe(nm->connid, true, false);
        _reg_request(nm->connid);
        break;
    case NETE_CONNERR:
        host_error("connect to center fail: %s", host_net_error());
        break;
    case NETE_SOCKERR:
        host_error("center disconnect: %s", host_net_error());
        host_node_disconnect(nm->connid);
        break;
    }
}

void
centerc_time(struct service* s) {
}
