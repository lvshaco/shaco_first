#include "host_service.h"
#include "host_gate.h"
#include "host_log.h"
#include "user_message.h"
#include <assert.h>
void
echo_usermsg(struct service* s, int id, void* msg, int sz) {
    struct gate_message* gm = msg;
    assert(gm->c);
    UM_CAST(UM_base, um, gm->msg);
    UM_SENDTOCLI(id, um, um->msgsz);
}
void
echo_net(struct service* s, struct gate_message* gm) {
    struct net_message* nm = gm->msg;
    switch (nm->type) {
    case NETE_SOCKERR:
    case NETE_TIMEOUT:
        host_info("close %d, %d", gm->c->connid, nm->type);
        break;
    }
}
