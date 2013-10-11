#ifndef __cnet_h__
#define __cnet_h__

#include "net_message.h"
#include "cli_message.h"

typedef void (*cnet_onconn)(struct net_message* nm);
typedef void (*cnet_onconnerr)(struct net_message* nm);
typedef void (*cnet_onsockerr)(struct net_message* nm);
typedef void (*cnet_handleum)(int id, int ut, struct UM_BASE* um);

void cnet_cb(cnet_onconn onconn, cnet_onconnerr onconnerr, 
             cnet_onsockerr onsockerr, cnet_handleum handleum);

int  cnet_init(int cmax);
void cnet_fini();
int  cnet_connect(const char* ip, uint16_t port, int ut);
int  cnet_send(int id, void* um, int sz);
int  cnet_poll(int timeout);
int  cnet_subscribe(int id, int read, int write);

#endif
