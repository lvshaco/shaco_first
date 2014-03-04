#include "sh.h"
#include "hashid.h"
#include "freeid.h"
#include "msg_server.h"
#include "msg_client.h"

static const char *PASSWD = "7c4a8d09ca3762af61e59520943dc26494f8941b";
static const char *ACC_PREFIX = "wa_account_";
static const char *CHAR_PREFIX = "wa_robotcli_";
static const uint32_t ITEMS[] = 
{100501, 221318, 410101, 200501, 210507/*, 210709, 221219, 221420*/};

#define HEART_BEAT 3

#define S_INVALID 0
#define S_CONNECT_ROUTE 1
#define S_REQUEST_GATE 2
#define S_CONNECT_GATE 3
#define S_LOGIN_ACCOUNT 4
#define S_CREATE_CHAR 5
#define S_HALL 6
#define S_ROOM 7
#define S_START 8
#define S_OVER 9
#define S_FAIL 10

#define CONN_ROUTE 1
#define CONN_GATE  2

#define DISCONN_CONNERR 1
#define DISCONN_SOCKERR 2
#define DISCONN_ACTIVE  3

struct client {
    int connid;
    int id;
    char account[ACCOUNT_NAME_MAX+1];
    struct chardata cdata;
    int err;
    int status;
    uint64_t last_send_time;
    uint64_t send_package;
    uint64_t send_bytes;
    uint64_t recv_package;
    uint64_t recv_bytes;

    uint64_t last_sync_pos;
    uint64_t last_use_item;

    int item_index;
};

static void 
client_init(struct client *c, int id) {
    memset(c, 0, sizeof(*c));
    c->connid = -1;
    c->id = id;
    snprintf(c->account, sizeof(c->account), "%s%d", ACC_PREFIX, id);
}

static inline void
client_bind_connection(struct client *c, int connid) {
    assert(c->connid == -1);
    c->connid = connid;
}

static inline void
client_disconnect(struct client *c) { 
    assert(c->connid != -1);
    c->connid = -1;
}

static inline void
client_send(struct client *c, void *msg, int sz) {
    assert(c->connid != -1);

    uint8_t *tmp = malloc(sz+2);
    sh_to_littleendian16(sz, tmp);
    memcpy(tmp+2, msg, sz);
    assert(!sh_net_send(c->connid, tmp, sz+2));

    c->last_send_time = sh_timer_now();
    c->send_package++;
    c->send_bytes += sz;
}

static inline void
client_heartbeat(struct client *c) {
    UM_DEFFIX(UM_HEARTBEAT, hb);
    client_send(c, hb, sizeof(*hb));
    sh_trace("Client %d heart beat", c->id);
}

static inline void
client_fail(struct client *c, int err, const char *str) {
    c->status = S_FAIL;
    c->err = err;
    sh_error("Client %d fail(%d), for %s", c->id, err, str);
}

static inline void
client_request_gate(struct client *c) {
    c->status = S_REQUEST_GATE;

    UM_DEFFIX(UM_GATEADDRREQ, req);
    client_send(c, req, sizeof(*req));
    sh_trace("Client %d request gate addresss", c->id);
}

static inline void
client_login_account(struct client *c) {
    c->status = S_LOGIN_ACCOUNT;

    UM_DEFFIX(UM_LOGINACCOUNT, la);
    sh_strncpy(la->account, c->account, sizeof(la->account));
    sh_strncpy(la->passwd, PASSWD, sizeof(la->passwd));
    client_send(c, la, sizeof(*la));
    sh_trace("Client %d request login account", c->id);
}

static inline void
client_create_char(struct client *c) {
    c->status = S_CREATE_CHAR;

    UM_DEFFIX(UM_CHARCREATE, create);
    snprintf(create->name, sizeof(create->name), "%s%u", CHAR_PREFIX, c->id);
    client_send(c, create, sizeof(*create));
    sh_trace("Client %d request create character(%s)", c->id, create->name);
}

static inline void
client_info(struct client *c, const struct chardata *cdata) { 
    c->status = S_HALL;
    
    c->cdata = *cdata;
    sh_trace("Client %d info(%u, %s)", c->id, cdata->charid, cdata->name);
}
/*
static void
client_buy_role(struct client *c, uint32_t roleid) {
    UM_DEFFIX(UM_BUYROLE, buy);
    buy->roleid = roleid;
    client_send(c, buy, sizeof(*buy));
    sh_trace("Client %d requset buy role(%d)", c->id, roleid);
}
*/
static inline void
client_play(struct client *c, int type) { 
    UM_DEFFIX(UM_PLAY, play);
    play->type = type;
    client_send(c, play, sizeof(*play));
    sh_trace("Client %d requset play(%d)", c->id, type);
}

static inline void
client_roominfo(struct client *c, struct UM_GAMEINFO *gi) {
    c->status = S_ROOM;
    sh_trace("Client %d game info: load leasttime: %d, nmember %d", 
            c->id, gi->load_least_time, gi->nmember);
    int i;
    for (i=0; i<gi->nmember; ++i) {
        sh_trace("  member %i: %u, %s", i, gi->members[i].charid, gi->members[i].name);
    }
}

static inline void
client_loadok(struct client *c) {
    UM_DEFFIX(UM_GAMELOADOK, ok);
    client_send(c, ok, sizeof(*ok));
    sh_trace("Client %d notify load ok", c->id);

}

static inline void
client_start(struct client *c) {
    c->status = S_START;
    sh_trace("Client %d start", c->id);
}

static inline void
client_over(struct client *c) {
    c->status = S_OVER;
    sh_trace("Client %d over", c->id);
}

static inline void
client_use_item(struct client *c, uint32_t itemid) {
    UM_DEFFIX(UM_USEITEM, ui);
    ui->itemid = itemid;
    client_send(c, ui, sizeof(*ui));
    sh_trace("Client %d requset use item(%u)", c->id, itemid);
}

static inline void
client_sync_position(struct client *c, uint32_t depth) {
    UM_DEFFIX(UM_GAMESYNC, sync);
    sync->charid = c->cdata.charid;
    sync->depth = depth;
}

struct robotcli {
    struct freeid fi;
    struct client* clients;
    int max;
    int startid;

    int nconnect_route_ok;
    int nconnect_route_fail;
    int nconnect_gate_ok;
    int nconnect_gate_fail;
    int nlogin_ok;
    int nlogin_fail;

    int nplay_req;
    int nplay_ok;
    int nplay_fail;
    int ngame_info;
    int ngame_enter;
    int ngame_start;
    int ngame_over; 
};

static void
check_total_connect_route_ok(struct robotcli *self) {
    if (self->nconnect_route_ok + self->nconnect_route_fail == self->max) {
        sh_info("Total(%d + %d) connect route ok", 
                self->nconnect_route_ok, self->nconnect_route_fail);
    }
}

static void
check_total_connect_gate_ok(struct robotcli *self) {
    if (self->nconnect_gate_ok + self->nconnect_gate_fail == self->max) {
        sh_info("Total(%d + %d) connect gate ok", 
                self->nconnect_gate_ok, self->nconnect_gate_fail);
    }
}

static void
check_total_login_ok(struct robotcli *self) {
    if (self->nlogin_ok + self->nlogin_fail == self->nconnect_gate_ok) {
        sh_info("Total(%d + %d) login ok, %d", 
                self->nlogin_ok, self->nlogin_fail, self->nconnect_gate_ok);
    }
}

static void
on_connect(struct robotcli* self, int connid) {
    int id = freeid_alloc(&self->fi, connid);
    assert(id >= 0 && id < self->max);

    struct client* c = &self->clients[id];
    client_bind_connection(c, connid);
    sh_net_subscribe(connid, true);
    
    switch (c->status) {
    case S_CONNECT_ROUTE:
        self->nconnect_route_ok++;
        check_total_connect_route_ok(self);
        client_request_gate(c);
        break;
    case S_CONNECT_GATE:
        self->nconnect_gate_ok++;
        check_total_connect_gate_ok(self);
        client_login_account(c); 
        break;
    default:
        assert(0);
        break;
    }
}

static void
on_connecterr(struct robotcli *self, int ut, int err) {
    switch (ut) {
    case CONN_ROUTE:
        self->nconnect_route_fail++;
        check_total_connect_route_ok(self);
        break;
    case CONN_GATE:
        self->nconnect_gate_fail++;
        check_total_connect_gate_ok(self);
        break;
    default:
        assert(0);
        break;
    }
}

static void
on_disconnect(struct robotcli* self, int connid, int type, int err) { 
    int id = freeid_free(&self->fi, connid);
    if (id < 0) {
        sh_panic("on disconnect: connid %d, type %d, err %d", connid, type, err);
    }
    assert(id >= 0 && id < self->max);

    struct client* c = &self->clients[id];
    client_disconnect(c);

    if (type == DISCONN_SOCKERR) {
        if (c->status == S_CONNECT_GATE) {
            sh_error("Client %d gate sockerr(%d)", c->id, err);
        }
    }
}

static int
client_connect_route(struct module *s, struct client *c, const char *ip, int port) {
    c->status = S_CONNECT_ROUTE;

    if (!sh_net_connect(ip, port, false, MODULE_ID, CONN_ROUTE)) {
        return 0;
    } else {
        sh_trace("Client %d connect route fail", c->id);
        return 1;
    }
}

static int
client_connect_gate(struct module *s, struct client *c, const char *ip, int port) {
    struct robotcli *self = MODULE_SELF;
    if (c->connid != -1) {
        sh_net_close_socket(c->connid, true);
        on_disconnect(self, c->connid, DISCONN_ACTIVE, 0);
    }
    c->status = S_CONNECT_GATE;

    if (!sh_net_connect(ip, port, false, MODULE_ID, CONN_GATE)) {
        return 0;
    } else {
        sh_trace("Client %d connect gate fail", c->id);
        return 1;
    } 
}

static void 
client_handle(struct module *s, struct client *c, void *msg, int sz) {
    struct robotcli *self = MODULE_SELF;
    c->recv_package++;
    c->recv_bytes += sz;

    UM_CAST(UM_BASE, base, msg);

    sh_trace("Client %d recv %u sz %d", c->id, base->msgid, sz);
    
    switch (base->msgid) {
    case IDUM_GATEADDR: {
        UM_CAST(UM_GATEADDR, addr, base);
        client_connect_gate(s, c, addr->ip, addr->port); 
        break;
        }
    case IDUM_GATEADDRFAIL: {
        client_fail(c, SERR_REQGATE, "request gate address");
        break;
        }
    case IDUM_LOGINACCOUNTFAIL: {
        UM_CAST(UM_LOGINACCOUNTFAIL, fail, base);
        client_fail(c, fail->err, "login account");
        self->nlogin_fail++;
        check_total_login_ok(self);
        break;
        }
    case IDUM_LOGOUT: {
        UM_CAST(UM_LOGOUT, lo, base);
        client_fail(c, lo->err, "logout gate");
        self->nlogin_fail++;
        check_total_login_ok(self);
        break;
        }
    case IDUM_LOGINFAIL: {
        UM_CAST(UM_LOGINFAIL, fail, base); 
        if (fail->err == SERR_NOCHAR ||
            fail->err == SERR_NAMEEXIST) {
            client_create_char(c);
        } else {
            client_fail(c, fail->err, "login gate");
            self->nlogin_fail++;
            check_total_login_ok(self);
        }
        break;
        }
    case IDUM_CHARINFO: {
        UM_CAST(UM_CHARINFO, ci, base);
        client_info(c, &ci->data);
        self->nlogin_ok++;
        check_total_login_ok(self);

        client_play(c, 0);
        self->nplay_req++;
        if (self->nplay_req == self->max) {
            sh_info("Total(%d) play req", self->nplay_req);
        }
        break;
        }
    case IDUM_PLAYFAIL: {
        UM_CAST(UM_PLAYFAIL, pf, base);
        client_fail(c, pf->err, "play");
        self->nplay_fail++;
        break;
        }
    case IDUM_PLAYWAIT: {
        UM_CAST(UM_PLAYWAIT, pw, base);
        sh_trace("Client %d play wait, timeout %d", c->id, pw->timeout);
        break;
        }
    case IDUM_GAMEINFO: {
        UM_CAST(UM_GAMEINFO, gi, base); 
        client_roominfo(c, gi); 
        self->ngame_info++;
        if (self->ngame_info == self->max) {
            sh_info("Total(%d) game info ok", self->ngame_info);
        }
        client_loadok(c);
        self->nplay_ok++;
        if (self->nplay_ok == self->max) {
            sh_info("Total(%d) play ok", self->nplay_ok);
        }
        break;
        }
    case IDUM_GAMEMEMBER: {
        UM_CAST(UM_GAMEMEMBER, gm, base); 
        sh_trace("Client %d add member(%u, %s)", c->id, gm->member.charid, gm->member.name);
        break;
        }
    case IDUM_GAMEENTER: {
        //UM_CAST(UM_GAMEENTER, ge, base);
        self->ngame_enter++;
        if (self->ngame_enter == self->max) {
            sh_info("Total(%d) game enter ok", self->ngame_enter);
        }
        sh_trace("Client %d game enter", c->id);
        break;
        }
    case IDUM_GAMESTART: {
        //UM_CAST(UM_GAMESTART, gs, base);
        client_start(c);
        self->ngame_start++;
        if (self->ngame_start == self->max) {
            sh_info("Total(%d) game start ok", self->ngame_start);
        }
        sh_trace("Client %d game start", c->id);
        if (c->cdata.accid % 2 == 0) {
            client_use_item(c, 2);
        }
        break;
        }
    case IDUM_ITEMEFFECT: {
        UM_CAST(UM_ITEMEFFECT, ie, base);
        sh_trace("Client %d item effect %u, to char %u", c->id, ie->itemid, ie->charid);
        }
        break;
    case IDUM_ROLEINFO: {
        UM_CAST(UM_ROLEINFO, ri, base);
        sh_trace("Client %d update roleinfo: %u", c->id, ri->detail.charid);
        }
        break;
    case IDUM_GAMEOVER: {
        UM_CAST(UM_GAMEOVER, go, base);
        client_over(c);
        self->ngame_over++;
        if (self->ngame_over == self->max) {
            sh_info("Total(%d) game over ok", self->ngame_over);
        }
        sh_trace("************Client %d GAME OVER*****************", c->id);
        sh_trace("room type: %d, member count: %d", go->type, go->nmember);
        sh_trace("rank charid depth oxygenitem item bao exp coin score");
        int i;
        for (i=0; i<go->nmember; ++i) {
            sh_trace("%d. [%u] %u %u %u %u %u %u %u", i+1, 
                    go->stats[i].charid,
                    go->stats[i].depth,
                    go->stats[i].noxygenitem,
                    go->stats[i].nitem,
                    go->stats[i].nbao,
                    go->stats[i].exp,
                    go->stats[i].coin,
                    go->stats[i].score);
        }
        sh_trace("******************************************");
        client_play(c, 0);
        break;
        }
    }
}

struct robotcli*
robotcli_create() {
    struct robotcli* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
robotcli_free(struct robotcli* self) {
    if (self == NULL)
        return;
    uint64_t send_package_all = 0, 
             send_bytes_all = 0, 
             recv_package_all = 0, 
             recv_bytes_all = 0;
    struct client *c;
    int i;
    for (i=0; i<self->max; ++i) {
        c = &self->clients[i];
        send_package_all += c->send_package;
        send_bytes_all   += c->send_bytes;
        recv_package_all += c->recv_package;
        recv_bytes_all   += c->recv_bytes;
    }
    sh_info("|**************************robot stat****************************|");
    sh_info("route connection %d + %d", self->nconnect_route_ok, self->nconnect_route_fail);
    sh_info("gate connection  %d + %d", self->nconnect_gate_ok,  self->nconnect_gate_fail);
    sh_info("gate login %d + %d",       self->nlogin_ok, self->nlogin_fail);
    sh_info("send_package_agv %u", (uint32_t)(send_package_all/self->max));
    sh_info("recv_package_agv %u", (uint32_t)(recv_package_all/self->max));
    sh_info("send_bytes_agv   %u", (uint32_t)(send_bytes_all/self->max));
    sh_info("recv_bytes_agv   %u", (uint32_t)(recv_bytes_all/self->max));
    sh_info("play req   %u", self->nplay_req);
    sh_info("play ok    %u", self->nplay_ok);
    sh_info("play fail  %u", self->nplay_fail);
    sh_info("game info  %u", self->ngame_info);
    sh_info("game enter %u", self->ngame_enter);
    sh_info("game start %u", self->ngame_start);
    sh_info("game over  %u", self->ngame_over);
    sh_info("|****************************************************************|");
    freeid_fini(&self->fi);
    free(self->clients);
    free(self);
}

static void
clients_init(struct module *s) {
    struct robotcli *self = MODULE_SELF;
    const char *ip = sh_getstr("route_ip", "0");
    int port = sh_getint("route_port", 0);

    int i;
    for (i=0; i<self->max; ++i) { 
        client_init(&self->clients[i], self->startid+i);
        client_connect_route(s, &self->clients[i], ip, port);
    }
}

int
robotcli_init(struct module *s) {
    struct robotcli* self = MODULE_SELF;

    self->startid = sh_getint("robotcli_client_startid", 0);
    sh_info("robotcli_client_startid %d", self->startid);

    int hmax = sh_getint("sh_connmax", 0);
    int cmax = sh_getint("robotcli_client_max", 0); 
    if (cmax <= 0) {
        sh_error("Client count invalid");
        return 1;
    }
    if (cmax > hmax) {
        sh_error("Client max over connection max");
        return 1;
    }
    self->max = cmax;
    self->clients = malloc(sizeof(struct client) * cmax);
    memset(self->clients, 0, sizeof(struct client) * cmax);
    freeid_init(&self->fi, cmax, hmax);
   
    clients_init(s);
    
    sh_timer_register(MODULE_ID, 200);
    return 0;
}

static inline struct client*
client_get(struct robotcli* self, int connid) {
    int id = freeid_find(&self->fi, connid);
    assert(id >= 0 && id < self->max);
    struct client* c = &self->clients[id];
    assert(c->connid != -1);
    return c;
}

static void
read_msg(struct module *s, struct net_message* nm) {
    struct robotcli* self = MODULE_SELF;
    int id = nm->connid;
    struct client* c = client_get(self, id);
    assert(c);
    assert(c->connid == id);
    int step = 0;
    int drop = 1;
    int err;
    for (;;) {
        err = 0; 
        struct mread_buffer buf;
        int nread = sh_net_read(id, drop==0, &buf, &err);
        if (nread <= 0) {
            if (!err)
                return;
            else
                goto errout;
        }
        for (;;) {
            if (buf.sz < 2) {
                break;
            }
            uint16_t sz = sh_from_littleendian16((uint8_t*)buf.ptr) + 2;
            if (buf.sz < sz) {
                break;
            }
            client_handle(s, c, buf.ptr+2, sz-2);
            buf.ptr += sz;
            buf.sz  -= sz;
            if (++step > 10) {
                sh_net_dropread(id, nread-buf.sz);
                return;
            }
        }
        if (err) {
            sh_net_close_socket(id, true);
            goto errout;
        }
        drop = nread - buf.sz;
        sh_net_dropread(id, drop);       
    }
    return;
errout:
    nm->type = NETE_SOCKERR;
    nm->error = err;
    module_net(nm->ud, nm);
}

void
robotcli_net(struct module* s, struct net_message* nm) {
    struct robotcli* self = MODULE_SELF;
    switch (nm->type) {
    case NETE_READ:
        read_msg(s, nm);
        break;
    case NETE_CONNECT:
        on_connect(self, nm->connid);
        break;
    case NETE_CONNERR:
        on_connecterr(self, nm->ut, nm->error);
        break;
    case NETE_SOCKERR:
        on_disconnect(self, nm->connid, DISCONN_SOCKERR, nm->error);
        break;
    }
}

void
robotcli_time(struct module* s) {
    struct robotcli* self = MODULE_SELF; 
    struct client *c;
   
    uint64_t now = sh_timer_now();
    int i;
    for (i=0; i<self->max; ++i) {
        c = &self->clients[i];
        if (c->connid != -1 &&
            c->status >= S_HALL && c->status <= S_ROOM) {
            if (now - c->last_send_time > HEART_BEAT) {
                client_heartbeat(c);
            }
        }
    }
    for (i=0; i<self->max; ++i) {
        c = &self->clients[i];
        if (c->connid != -1 &&
            c->status == S_START) {
            if (now - c->last_sync_pos >= 150) {
                client_sync_position(c, 5);
                c->last_sync_pos = now;
            }
            if (now - c->last_use_item >= 500) {
                // 221318, 100501
                if (c->item_index >= sizeof(ITEMS)/sizeof(ITEMS[0]))
                    c->item_index = 0;
                int itemid = ITEMS[c->item_index++];
                client_use_item(c, itemid);
                c->last_use_item = now;
            }
        }
    }
}
