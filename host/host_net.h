#ifndef __host_net_h__
#define __host_net_h__

#include <stdint.h>
#include <stdbool.h>
#include "net.h"

int host_net_init(int max);
void host_net_fini();
int host_net_listen(const char* addr, uint16_t port, int wbuffermax, int serviceid, int ut);
int host_net_connect(const char* addr, uint16_t port, bool block, int serviceid, int ut);
void host_net_poll(int timeout);
int host_net_readto(int id, void* buf, int space, int* e);
void* host_net_read(int id, int sz, int skip, int* e);
void host_net_dropread(int id, int skip);
int host_net_send(int id, void* data, int sz);
bool host_net_close_socket(int id, bool force);
int host_net_max_socket();
const char* host_net_error(int err);
int host_net_subscribe(int id, bool read);
int host_net_socket_address(int id, uint32_t* addr, uint16_t* port);
int host_net_socket_isclosed(int id);

#endif
