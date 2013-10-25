#ifndef __net_message_h__
#define __net_message_h__

#include <stdint.h>

#define NETE_INVALID -1
#define NETE_READ    0
#define NETE_ACCEPT  1 
#define NETE_CONNECT 2 
#define NETE_CONNERR 3 
#define NETE_SOCKERR 4
//#define NETE_WRIDONE 5
#define NETE_CONN_THEN_READ 6
#define NETE_TIMEOUT 7
#define NETE_LOGOUT 8
#define NETE_REDISREPLY 9

#define NETUT_TRUST 0 // or UNTRUST

struct net_message {
    int fd;
    int connid;
    int type;     // see NETE
    int error;
    int ud;
    int ut; // see NETUT_*
};

#endif
