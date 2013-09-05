#ifndef __host_net_h__
#define __host_net_h__

#include <stdint.h>
#include <stdbool.h>
#include "net.h"

#define net_message net_event

int host_net_init(int max);
void host_net_fini();
int host_net_listen(const char* addr, uint16_t port, int serviceid);
int host_net_connect(const char* addr, uint16_t port, bool block, int serviceid);
void host_net_poll(int timeout);
void* host_net_read(int id, int sz);
void host_net_dropread(int id);
int host_net_send(int id, void* data, int sz);
void host_net_close_socket(int id);
int host_net_error();

#endif
