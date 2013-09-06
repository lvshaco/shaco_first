#ifndef __NET_H__ 
#define __NET_H__
#include <stdbool.h>
#include <stdint.h>
#include "net_event.h"

#define NET_OK(r) (r[0] == '\0')

struct net;
struct net* net_create(int max, int block_size);
void net_free(struct net* self);

int net_listen(struct net* self, uint32_t addr, uint16_t port, int udata);
int net_connect(struct net* self, uint32_t addr, uint16_t port, bool block, int udata);
int net_poll(struct net* self, int timeout);
int net_getevents(struct net* self, struct net_event** e);
int net_subscribe(struct net* self, int id, bool read, bool write);

void* net_read(struct net* self, int id, int size);
void net_dropread(struct net* self, int id);
int net_send(struct net* self, int id, void* data, int sz);
void net_close_socket(struct net* self, int id);
const char* net_error(struct net* self);
int net_max_socket(struct net* self);

#endif
