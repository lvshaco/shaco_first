#ifndef __net_event_h__
#define __net_event_h__

#define NETE_INVALID -1
#define NETE_READ    0
#define NETE_ACCEPT  1 
#define NETE_CONNECT 2 
#define NETE_CONNECTERR 3 
#define NETE_CONNECT_THEN_READ 4 

struct net_event {
    int fd;
    int connid;
    int type;
    int udata;
};

#endif
