#include "cnet.h"
#include <stdio.h>
#include <assert.h>

static void
_heartbeat(int id) {
    UM_DEFFIX(UM_LOGIN, hb);
    cnet_send(id, hb, sizeof(*hb));
}
static void
_onconnect(struct net_message* nm) {
    printf("onconnect\n");
    int id = nm->connid;
    cnet_subscribe(id, 1, 0);
    _heartbeat(id);
}
static void
_onconnerr(struct net_message* nm) {
    printf("onconnerr: %d\n", nm->error);
}
static void
_onsockerr(struct net_message* nm) {
    printf("onsockerr: %d\n", nm->error);
}
static void 
_handleum(int id, int ut, struct UM_BASE* um) {
    assert(um->msgid == IDUM_LOGIN);
    printf("handleum\n");
    _heartbeat(id);
}
int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("usage: test ip port\n");
        return 1;
    }
    if (cnet_init(10)) {
        printf("cnet_init fail\n");
        return 1;
    }
    cnet_cb(_onconnect, 
            _onconnerr, 
            _onsockerr, 
            _handleum);
    const char* ip = argv[1];
    uint16_t port = strtoul(argv[2], NULL, 10);
    if (cnet_connect(ip, port, 0) < 0) {
        printf("connect fail\n");
        return 1;
    }
    for (;;) {
        cnet_poll(1);
    }
    cnet_fini();
    system("pause"); 
    return 0;
}
