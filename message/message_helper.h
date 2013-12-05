#ifndef __message_helper_h__
#define __message_helper_h__

static inline void
mread_throwerr(struct net_message* nm, int e) {
    if (e) {
        // error occur, route to net service
        nm->type = NETE_SOCKERR;
        nm->error = e;
        service_notify_net(nm->ud, nm);
    }
}

#endif
