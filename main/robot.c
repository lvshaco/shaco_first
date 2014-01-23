#include "cnet.h"
#include "cli_message.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#define TROUTE 0
#define TGATE 1
#define TMAX 2
static int SERVER[TMAX];
static struct UM_GATEADDR GATEADDR;
static struct chardata CHAR;
static char ACCOUNT[ACCOUNT_NAME_MAX];

static void
mylog(const char *fmt, ...) {
    time_t now = time(NULL);
    char buf[64];
    strftime(buf, sizeof(buf), "%H:%M:%S ", localtime(&now));
    fprintf(stderr, buf);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

static void
_server_init() {
    int i;
    for (i=0; i<TMAX; ++i) {
        SERVER[i] = -1;
    }
}
static inline int
_server_get(int t) {
    assert(t>=0 && t<TMAX);
    return SERVER[t];
}
static inline void
_server_set(int t, int id) {
    assert(t>=0 && t<TMAX);
    SERVER[t] = id;
}

#define _server_send(t, msg, sz) \
    cnet_send(SERVER[t], msg, sz);

static void
_request_gate(int id) {
    UM_DEFFIX(UM_GATEADDRREQ, req);
    _server_send(TROUTE, req, sizeof(*req));
    mylog("request gate address");
}

static void
_login_account(int id) {
    UM_DEFFIX(UM_LOGINACCOUNT, la);
    strncpy(la->account, ACCOUNT, sizeof(la->account)-1);
    strncpy(la->passwd, "123456", sizeof(la->passwd)-1);
    _server_send(TGATE, la, sizeof(*la));
    mylog("request login account");
}

static void
_onconnect(struct net_message* nm) {
    mylog("onconnect");
    int ut = nm->ut;
    int id = nm->connid;
    _server_set(ut, id);
  
    cnet_subscribe(id, 1);

    switch (ut) {
    case TROUTE:
        _request_gate(id);
        break;
    case TGATE:
        _login_account(id);
        break;
    }
}

static void
_onconnerr(struct net_message* nm) {
    mylog("onconnerr: %d, ut %d", nm->error, nm->ut);
    _server_set(nm->ut, -1);
}
static void
_onsockerr(struct net_message* nm) {
    mylog("onsockerr: %d, ut %d", nm->error, nm->ut);
    _server_set(nm->ut, -1);
}

static void
_createchar() {
    
    UM_DEFFIX(UM_CHARCREATE, cre);
    const char* prefix = "wa_char_";
    int len = strlen(prefix);
    strcpy(cre->name, prefix);
    int i;
    for (i=len; i<sizeof(cre->name)-1; ++i) {
        cre->name[i] = rand()%26 + 'A';
    }
    cre->name[i] = '\0';
    _server_send(TGATE, cre, sizeof(*cre));
    mylog("request create char: %s", cre->name);
}

static void
_play(int type) {
    UM_DEFFIX(UM_BUYROLE, buy);
    buy->roleid = 11;
    _server_send(TGATE, buy, sizeof(*buy));

    UM_DEFFIX(UM_PLAY, play);
    play->type = type;
    _server_send(TGATE, play, sizeof(*play));
    mylog("request play: %d", type);
}

static void
_loadok() {
    UM_DEFFIX(UM_GAMELOADOK, ok);
    _server_send(TGATE, ok, sizeof(*ok));
    mylog("notify load ok");

}

static void
_useitem(uint32_t id) {
    UM_DEFFIX(UM_USEITEM, ui);
    ui->itemid = id;
    _server_send(TGATE, ui, sizeof(*ui));
    mylog("requset use item: %u", id);
}

static void 
_handleum(int id, int ut, struct UM_BASE* um) {
    mylog("handleum: %d", um->msgid);
    switch (um->msgid) {
//    case 1500: {
//        static int I = 0;
//        I++;
//        if (I > 900)
//            cnet_disconnect(id);
//        }
//        break;
    case IDUM_GATEADDR: {
        UM_CAST(UM_GATEADDR, addr, um);
        GATEADDR = *addr;
        mylog("gate address: %s:%u", GATEADDR.ip, GATEADDR.port);
        mylog("connect to gate %s:%u", GATEADDR.ip, GATEADDR.port);
        if (cnet_connect(GATEADDR.ip, GATEADDR.port, TGATE) < 0) {
            mylog("connect gate fail");
        }
        break;
        }
    case IDUM_GATEADDRFAIL: {
        mylog("request gate address fail");
        break;
        }
    case IDUM_LOGINACCOUNTFAIL: {
        UM_CAST(UM_LOGINACCOUNTFAIL, fail, um);
        mylog("accout login fail: error#%d", fail->err);
        }
        break;
    case IDUM_LOGOUT: {
        UM_CAST(UM_LOGOUT, lo, um);
        mylog("gate logout: error %d", lo->err);
        break;
        }
    case IDUM_LOGINFAIL: {
        UM_CAST(UM_LOGINFAIL, fail, um); 
        if (fail->err == SERR_NOCHAR ||
            fail->err == SERR_NAMEEXIST) {
            _createchar();
        } else {
            mylog("gate login fail: error %d", fail->err);
        }
        }
        break;
    case IDUM_CHARINFO: {
        UM_CAST(UM_CHARINFO, ci, um);
        mylog("charinfo: id %u, name %s", ci->data.charid, ci->data.name);
        CHAR = ci->data;
        _play(0);
        break;
        }
    case IDUM_PLAYFAIL: {
        UM_CAST(UM_PLAYFAIL, pf, um);
        mylog("play fail: error %d", pf->err);
        break;
        }
    case IDUM_PLAYWAIT: {
        UM_CAST(UM_PLAYWAIT, pw, um);
        mylog("play wait: timeout %d", pw->timeout);
        break;
        }
    case IDUM_PLAYLOADING: {
        UM_CAST(UM_PLAYLOADING, pl, um);
        mylog("play loading: leasttime: %d, other(%u,%s)",
                pl->leasttime, pl->member.charid, pl->member.name);
        _loadok();
        break;
        }
    case IDUM_GAMEINFO: {
        UM_CAST(UM_GAMEINFO, gi, um);
        mylog("game info: nmember %d", gi->nmember);
        break;
        }
    case IDUM_GAMEMEMBER: {
        UM_CAST(UM_GAMEMEMBER, gm, um);
        mylog("add member id %u, name %s", gm->member.charid, gm->member.name);
        break;
        }
    case IDUM_GAMEENTER: {
        //UM_CAST(UM_GAMEENTER, ge, um);
        mylog("game enter");
        break;
        }
    case IDUM_GAMESTART: {
        //UM_CAST(UM_GAMESTART, gs, um);
        mylog("game start");
        if (CHAR.accid % 2 == 0) {
        //_useitem(2);
        _useitem(3);
        //_useitem(4);
        }
        }
        break;
    case IDUM_ITEMEFFECT: {
        UM_CAST(UM_ITEMEFFECT, ie, um);
        mylog("item effect %u, to char %u", ie->itemid, ie->charid);
        }
        break;
    case IDUM_ROLEINFO: {
        UM_CAST(UM_ROLEINFO, ri, um);
        mylog("update roleinfo: %u", ri->detail.charid);
        }
        break;
    case IDUM_GAMEOVER: {
        UM_CAST(UM_GAMEOVER, go, um);
        mylog("****************GAME OVER*****************");
        mylog("room type: %d, member count: %d", go->type, go->nmember);
        mylog("rank charid depth oxygenitem item bao exp coin score");
        int i;
        for (i=0; i<go->nmember; ++i) {
            mylog("%d. [%u] %u %u %u %u %u %u %u", i+1, 
                    go->stats[i].charid,
                    go->stats[i].depth,
                    go->stats[i].noxygenitem,
                    go->stats[i].nitem,
                    go->stats[i].nbao,
                    go->stats[i].exp,
                    go->stats[i].coin,
                    go->stats[i].score);
        }
        mylog("******************************************");
        break;
        }
    }
}

int main(int argc, char* argv[]) { 
    const char* ip;
    uint16_t port;
    if (argc > 1) {
        snprintf(ACCOUNT, sizeof(ACCOUNT), "wa_account_%s", argv[1]);
    } else {
        strncpy(ACCOUNT, "wa_account_1", sizeof(ACCOUNT)-1);
    }
    if (argc > 3) {
        ip = argv[2];
        port = strtoul(argv[3], NULL, 10);
    } else {
        ip = "192.168.1.140";
        port = 18100;
    }
    
    srand(time(NULL));
    _server_init();

    if (cnet_init(10)) {
        mylog("cnet_init fail");
        return 1;
    }
    cnet_cb(_onconnect, 
            _onconnerr, 
            _onsockerr, 
            _handleum);
    mylog("connect to route %s:%u", ip, port);
    if (cnet_connect(ip, port, TROUTE) < 0) {
        mylog("connect route fail");
        return 1;
    }
    for (;;) {
        cnet_poll(1);
    }
    cnet_fini();
    system("pause"); 
    return 0;
}
