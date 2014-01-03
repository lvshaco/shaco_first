#ifndef __sc_net_h__
#define __sc_net_h__

#include <stdint.h>
#include <stdbool.h>
#include "net.h"

int sc_net_listen(const char* addr, int port, int wbuffermax, int serviceid, int ut);
int sc_net_connect(const char* addr, int port, bool block, int serviceid, int ut);
int sc_net_block_connect(const char* addr, int port, int serviceid, int ut, int *err);
void sc_net_poll(int timeout);
int sc_net_readto(int id, void* buf, int space, int* e);
int sc_net_read(int id, bool force, struct mread_buffer* buf, int* e);
void sc_net_dropread(int id, int sz);
int sc_net_send(int id, void* data, int sz);
int sc_net_block_send(int id, void *data, int sz, int *err);
bool sc_net_close_socket(int id, bool force);
int sc_net_max_socket();
const char* sc_net_error(int err);
int sc_net_subscribe(int id, bool read);
int sc_net_socket_address(int id, uint32_t* addr, int* port);
int sc_net_socket_isclosed(int id);

#endif
