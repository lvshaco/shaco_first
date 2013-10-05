#ifndef __NET_H__ 
#define __NET_H__
#include <stdbool.h>
#include <stdint.h>
#include "net_message.h"

struct net;
struct net* net_create(int max, int block_size);
void net_free(struct net* self);

int net_listen(struct net* self, uint32_t addr, uint16_t port, int ud, int ut);
int net_connect(struct net* self, uint32_t addr, uint16_t port, bool block, int ud, int ut);
int net_poll(struct net* self, int timeout);
int net_getevents(struct net* self, struct net_message** e);
int net_subscribe(struct net* self, int id, bool read, bool write);

void* net_read(struct net* self, int id, int size, int skip);
void net_dropread(struct net* self, int id, int skip);
int net_send(struct net* self, int id, void* data, int sz);
void net_close_socket(struct net* self, int id);
const char* net_error(struct net* self, int err);
int net_errorid(struct net* self);
int net_max_socket(struct net* self);
int net_socket_address(struct net* self, int id, uint32_t* addr, uint16_t* port);
int net_socket_isclosed(struct net* self, int id);

#endif
