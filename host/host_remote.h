#ifndef __host_remote_h__
#define __host_remote_h__

struct host_message;

void remote_init();
void remote_fini();

int remote_create(int connid);
int remote_destroy(int connid);

void remote_handle(int connid, struct host_message* hmsg);
void remote_multicast(int sign, struct host_message* hmsg);
void remote_send(int id, struct host_message* hmsg);

#endif
