#ifndef __sh_net_h__
#define __sh_net_h__

#include <stdint.h>
#include <stdbool.h>
#include "net.h"

int sh_net_listen(const char* addr, int port, int wbuffermax, int serviceid, int ut, int *err);
int sh_net_connect(const char* addr, int port, bool block, int serviceid, int ut);
int sh_net_block_connect(const char* addr, int port, int serviceid, int ut, int *err);
void sh_net_poll(int timeout);
int sh_net_readto(int id, void* buf, int space, int* e);
int sh_net_read(int id, struct mread_buffer* buf, int* e);
void sh_net_dropread(int id, int sz);
int sh_net_send(int id, void* data, int sz);
int sh_net_block_send(int id, void *data, int sz, int *err);
bool sh_net_close_socket(int id, bool force);
int sh_net_max_socket();
const char* sh_net_error(int err);
int sh_net_subscribe(int id, bool read);
int sh_net_socket_address(int id, uint32_t* addr, int* port);
int sh_net_socket_isclosed(int id);

#endif
