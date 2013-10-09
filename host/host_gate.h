#ifndef __host_gate_h__
#define __host_gate_h__

#include <stdint.h>
#include <stdbool.h>

struct gate_client {
    bool connected;
    int connid;
    uint64_t active_time;
};

// bind gate_client to msg
struct gate_message {
    struct gate_client* c;
    void* msg;
};

int host_gate_init();
void host_gate_fini();
int host_gate_prepare(int cmax, int hmax);
struct gate_client* host_gate_acceptclient(int connid, uint64_t now);
int host_gate_disconnclient(struct gate_client* c, bool closesocket);
struct gate_client* host_gate_getclient(int connid);
struct gate_client* host_gate_firstclient();
int host_gate_maxclient();
int host_gate_clientid(struct gate_client* c);

#endif
