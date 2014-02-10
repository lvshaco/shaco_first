#include "sh.h"
#include "msg_server.h"

void
echo_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        case IDUM_GATE: {
            UM_CASTCK(UM_GATE, ga, msg, sz);
            UM_CAST(UM_BASE, sub, ga->wrap);
            switch (sub->msgid) {
            case IDUM_NETDISCONN: {
                UM_CAST(UM_NETDISCONN, nd, sub);
                sh_info("module echo : close id %d, type %d, err %d", 
                        ga->connid, nd->type, nd->err);
                break;
                }
            default:
                sh_module_send(MODULE_ID, source, type, msg, sz);
                break;
            break;
            }
            break;
            }
        }
        break;
        }
    }
}
