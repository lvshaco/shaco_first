#ifndef __sc_dispatcher_h__
#define __sc_dispatcher_h__

struct net_message;

#define SUBSCRIBE_MSG sc_dispatcher_subscribe

int sc_dispatcher_subscribe(int serviceid, int msgid);
int sc_dispatcher_publish(struct net_message* nm);
int sc_dispatcher_usermsg(void* msg, int sz); // expand

#endif
