#include "sc_service.h"
#include "sc_gate.h"
#include "sc_log.h"
#include "user_message.h"
#include <assert.h>

void
echo_usermsg(struct service* s, int id, void* msg, int sz) {
    struct gate_message* gm = msg;
    assert(gm->c);
    UM_CAST(UM_BASE, um, gm->msg);
    assert(um->msgid == 100);
    UM_SENDTOCLI(id, um, um->msgsz);
}
void
echo_net(struct service* s, struct gate_message* gm) {
    struct net_message* nm = gm->msg;
    switch (nm->type) {
    case NETE_SOCKERR:
    case NETE_TIMEOUT:
        sc_info("echo_net close id %d, type %d, err %d", 
                gm->c->connid, nm->type, nm->error);
        break;
    }
}
