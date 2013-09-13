#ifndef __host_dispatcher_h__
#define __host_dispatcher_h__

struct net_message;

#define SUBSCRIBE_MSG host_dispatcher_subscribe

int host_dispatcher_init();
void host_dispatcher_fini();
int host_dispatcher_subscribe(int serviceid, int msgid);
int host_dispatcher_publish(struct net_message* nm);

#endif
