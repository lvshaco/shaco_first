#ifndef __sc_gate_h__
#define __sc_gate_h__

#include <stdint.h>
#include <stdbool.h>

#define GATE_CLIENT_FREE        0
#define GATE_CLIENT_CONNECTED   1
#define GATE_CLIENT_LOGINED     2
#define GATE_CLIENT_LOGOUTED    3

#define GATE_EVENT_ONACCEPT  0
#define GATE_EVENT_ONDISCONN 1

struct gate_client {
    int connid;
    int status;
    uint64_t active_time;
};

// bind gate_client to msg
struct gate_message {
    struct gate_client* c;
    void* msg;
};

int sc_gate_prepare(int cmax, int hmax);
struct gate_client* sc_gate_acceptclient(int connid);
void sc_gate_loginclient(struct gate_client* c);
bool sc_gate_disconnclient(struct gate_client* c, bool force);
struct gate_client* sc_gate_getclient(int connid);
struct gate_client* sc_gate_firstclient();
int sc_gate_maxclient();
int sc_gate_usedclient();
int sc_gate_clientid(struct gate_client* c);

#endif
