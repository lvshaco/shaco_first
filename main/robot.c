#include "cnet.h"
#include "cli_message.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#define TLOGIN 0
#define TGATE 1
#define TGAME 2
#define TMAX 3
static int SERVER[TMAX];
static struct UM_NOTIFYGAME GAMEADDR;
static struct UM_NOTIFYGATE GATEADDR;
static struct chardata CHAR;
static char ACCOUNT[ACCOUNT_NAME_MAX];
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
_login_account(int id) {
    UM_DEFFIX(UM_LOGINACCOUNT, la);
    strncpy(la->account, ACCOUNT, sizeof(la->account)-1);
    strncpy(la->passwd, "123456", sizeof(la->passwd)-1);
    _server_send(TLOGIN, la, sizeof(*la));
    printf("request login account\n");
}

static void
_login_gate(int id) {
    UM_DEFFIX(UM_LOGIN, lo);
    lo->accid = GATEADDR.accid;
    lo->key = GATEADDR.key;
    strcpy(lo->account, ACCOUNT);
    _server_send(TGATE, lo, sizeof(*lo));
    printf("request login gate\n");
}

static void
_login_game(int id) {
    UM_DEFFIX(UM_GAMELOGIN, gl);
    gl->charid = CHAR.charid;
    gl->roomid = GAMEADDR.roomid;
    gl->roomkey = GAMEADDR.key;
    _server_send(TGAME, gl, sizeof(*gl));
    printf("request login game: charid %u, roomid %u, key %d\n", 
            gl->charid, gl->roomid, gl->roomkey);
}

static void
_onconnect(struct net_message* nm) {
    printf("onconnect\n");
    int ut = nm->ut;
    int id = nm->connid;
    _server_set(ut, id);
  
    cnet_subscribe(id, 1);

    switch (ut) {
    case TLOGIN:
        _login_account(id);
        break;
    case TGATE:
        _login_gate(id);
        break;
    case TGAME:
        _login_game(id);
        break;
    }
}

static void
_onconnerr(struct net_message* nm) {
    printf("onconnerr: %d, ut %d\n", nm->error, nm->ut);
    _server_set(nm->ut, -1);
}
static void
_onsockerr(struct net_message* nm) {
    printf("onsockerr: %d, ut %d\n", nm->error, nm->ut);
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
    printf("request create char: %s\n", cre->name);
}

static void
_play(int type) {
    UM_DEFFIX(UM_BUYROLE, buy);
    buy->roleid = 11;
    _server_send(TGATE, buy, sizeof(*buy));

    UM_DEFFIX(UM_PLAY, play);
    play->type = type;
    _server_send(TGATE, play, sizeof(*play));
    printf("request play: %d\n", type);
}

static void
_useitem(uint32_t id) {
    UM_DEFFIX(UM_USEITEM, ui);
    ui->itemid = id;
    _server_send(TGAME, ui, sizeof(*ui));
    printf("requset use item: %u\n", id);
}

static void 
_handleum(int id, int ut, struct UM_BASE* um) {
    printf("handleum: %d\n", um->msgid);
    switch (um->msgid) {
/*    case 1500: {
        static int I = 0;
        I++;
        if (I > 900)
            cnet_disconnect(id);
        }
        break;
*/
    case IDUM_LOGINACCOUNTFAIL: {
        UM_CAST(UM_LOGINACCOUNTFAIL, fail, um);
        printf("accout login fail: error#%d\n", fail->error);
        }
        break;
    case IDUM_NOTIFYGATE: {
        //_login_account(id);
        UM_CAST(UM_NOTIFYGATE, g, um);
        printf("accid %u, gate address: %0x:%u, key: %llu\n", 
                g->accid, g->addr, g->port, (unsigned long long int)g->key);
        GATEADDR = *g;
        if (cnet_connecti(GATEADDR.addr, GATEADDR.port, TGATE) < 0) {
            printf("connect gate fail\n");
        }
        }
        break;
    case IDUM_LOGOUT: {
        UM_CAST(UM_LOGOUT, lo, um);
        printf("gate logout: error %d\n", lo->error);
        break;
        }
    case IDUM_LOGINFAIL: {
        UM_CAST(UM_LOGINFAIL, fail, um); 
        if (fail->error == SERR_NOCHAR ||
            fail->error == SERR_NAMEEXIST) {
            _createchar();
        } else {
            printf("gate login fail: error %d\n", fail->error);
        }
        }
        break;
    case IDUM_CHARINFO: {
        UM_CAST(UM_CHARINFO, ci, um);
        printf("charinfo: id %u, name %s\n", ci->data.charid, ci->data.name);
        CHAR = ci->data;
        _play(0);
        break;
        }
    case IDUM_PLAYFAIL: {
        UM_CAST(UM_PLAYFAIL, pf, um);
        printf("play fail: error %d\n", pf->error);
        break;
        }
    case IDUM_PLAYWAIT: {
        UM_CAST(UM_PLAYWAIT, pw, um);
        printf("play wait: timeout %d\n", pw->timeout);
        break;
        }
    case IDUM_PLAYLOADING: {
        UM_CAST(UM_PLAYLOADING, pl, um);
        printf("play loading: leasttime: %d, other(%u,%s)\n",
                pl->leasttime, pl->member.charid, pl->member.name);
        break;
        }
    case IDUM_NOTIFYGAME: {
        UM_CAST(UM_NOTIFYGAME, gn, um);
        printf("game address: %0x:%u, key: %u\n", gn->addr, gn->port, gn->key);
        GAMEADDR = *gn;
        if (cnet_connecti(GAMEADDR.addr, GAMEADDR.port, TGAME) < 0) {
            printf("connect game fail\n");
        }
        break;
        }
    case IDUM_GAMELOGINFAIL: {
        UM_CAST(UM_GAMELOGINFAIL, fail, um);
        printf("game login fail: error %d\n", fail->error);
        break;
        }
    case IDUM_GAMELOGOUT: {
        //UM_CAST(UM_GAMELOGOUT, lo, um);
        printf("game logout\n");
        break;
        }
    case IDUM_GAMEINFO: {
        UM_CAST(UM_GAMEINFO, gi, um);
        printf("game info: nmember %d\n", gi->nmember);
        break;
        }
    case IDUM_GAMEENTER: {
        //UM_CAST(UM_GAMEENTER, ge, um);
        printf("game enter\n");
        break;
        }
    case IDUM_GAMESTART: {
        //UM_CAST(UM_GAMESTART, gs, um);
        printf("game start\n");
        if (CHAR.accid % 2 == 0) {
        //_useitem(2);
        _useitem(3);
        //_useitem(4);
        }
        }
        break;
    case IDUM_ITEMEFFECT: {
        UM_CAST(UM_ITEMEFFECT, ie, um);
        printf("item effect %u, to char %u\n", ie->itemid, ie->charid);
        }
        break;
    case IDUM_ROLEINFO: {
        UM_CAST(UM_ROLEINFO, ri, um);
        printf("update roleinfo: %u\n", ri->detail.charid);
        }
        break;
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
        port = 18600;
    }
    
    srand(time(NULL));
    _server_init();

    if (cnet_init(10)) {
        printf("cnet_init fail\n");
        return 1;
    }
    cnet_cb(_onconnect, 
            _onconnerr, 
            _onsockerr, 
            _handleum);
    printf("connect to %s:%u\n", ip, port);
    if (cnet_connect(ip, port, TLOGIN) < 0) {
        printf("connect gate fail\n");
        return 1;
    }
    for (;;) {
        cnet_poll(1);
    }
    cnet_fini();
    system("pause"); 
    return 0;
}
