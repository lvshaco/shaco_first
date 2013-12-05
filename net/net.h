#ifndef __NET_H__ 
#define __NET_H__
#include <stdbool.h>
#include <stdint.h>
#include "net_message.h"

// must be negative, positive for system error number
//#define OK 0
#define NET_ERR_EOF         -1
#define NET_ERR_MSG         -2
#define NET_ERR_NOSOCK      -3
#define NET_ERR_CREATESOCK  -4
#define NET_ERR_WBUFOVER    -5
#define NET_ERR_NOBUF       -6

struct mread_buffer {
    void* ptr;
    int sz;
};

struct net;
struct net* net_create(int max, int rbuffer);
void net_free(struct net* self);

int net_listen(struct net* self, uint32_t addr, uint16_t port, int wbuffermax, int ud, int ut);
int net_connect(struct net* self, uint32_t addr, uint16_t port, bool block, int wbuffermax, int ud, int ut, struct net_message* nm);
int net_poll(struct net* self, int timeout);
int net_getevents(struct net* self, struct net_message** e);
int net_subscribe(struct net* self, int id, bool read);

int net_readto(struct net* self, int id, void* buf, int space, int* e);
int net_read(struct net* self, int id, bool force, struct mread_buffer* buf, int* e);
void net_dropread(struct net* self, int id, int sz);

int net_send(struct net* self, int id, void* data, int sz, struct net_message* nm);
bool net_close_socket(struct net* self, int id, bool force);
const char* net_error(struct net* self, int err);
int net_max_socket(struct net* self);
int net_socket_address(struct net* self, int id, uint32_t* addr, uint16_t* port);
int net_socket_isclosed(struct net* self, int id);

#endif
