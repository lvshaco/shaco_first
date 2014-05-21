#include "hall_player.h"
#include "hall.h"
#include "hall_playerdb.h"
#include "msg_server.h"
#include "msg_client.h"

static inline void
free_player(struct hall *self, struct player *pr) {
    sh_hash_remove(&self->acc2player, pr->data.accid);
    free(pr);
}

int 
hall_player_init(struct hall *self) {
    sh_hash_init(&self->acc2player, 1);
    return 0;
}

void
hall_player_fini(struct hall *self) {
    //sh_hash_foreach(&self->acc2player, free);
    sh_hash_fini(&self->acc2player);
}

static void 
login(struct module *s, int source, uint32_t accid) {
    struct hall *self = MODULE_SELF;
    struct player *pr = sh_hash_find(&self->acc2player, accid);
    if (pr) {
        sh_warning("Player %u relogin, free old", accid);
        free_player(self, pr); // just free old
    }
    pr = malloc(sizeof(*pr));
    memset(pr, 0, sizeof(*pr));
    pr->watchdog_source = source;
    pr->status = PS_QUERYCHAR;
    pr->data.accid = accid;
    assert(!sh_hash_insert(&self->acc2player, accid, pr));

    hall_gamelog(s, self->charactionlog_handle, "LOGIN,%u,%u", 
            accid, sh_timer_now()/1000);

    if (hall_playerdb_send(s, pr, PDB_QUERY)) {
        sh_trace("Player %u login fail, no db", accid);
        hall_notify_logout(s, pr, SERR_NODB);
        free_player(self, pr);
    } else {
        sh_trace("Player %u login", accid);
    }
}

static void 
logout(struct module *s, struct player *pr) {
    struct hall *self = MODULE_SELF;
    sh_trace("Player %u logout, status %d", UID(pr), pr->status);
    if (pr->status == PS_WAITING ||
        pr->status == PS_ROOM) {
        UM_DEFWRAP(UM_MATCH, ma, UM_LOGOUT, lo);
        ma->uid = UID(pr);
        lo->err = SERR_OK;
        sh_handle_send(MODULE_ID, self->match_handle, MT_UM, ma, sizeof(*ma)+sizeof(*lo)); 
    } 
    hall_playerdb_save(s, pr, true);
    hall_gamelog(s, self->charactionlog_handle, "LOGOUT,%u,%u", 
            pr->data.accid, sh_timer_now()/1000);

    free_player(self, pr); 
}

static void 
create_char(struct module *s, struct player *pr, const char *name) {
    struct hall *self = MODULE_SELF;
    if (pr->status == PS_WAITCREATECHAR) { 
        sh_strncpy(pr->data.name, name, sizeof(pr->data.name));
        sh_trace("Player %u create character %s", UID(pr), pr->data.name);
        if (hall_playerdb_send(s, pr, PDB_CHECKNAME)) {
            hall_notify_logout(s, pr, SERR_NODB);
            free_player(self, pr);
        }
    }
}

void 
hall_player_main(struct module *s, int source, struct player *pr, const void *msg, int sz) {
    UM_CAST(UM_BASE, base, msg);
    switch (base->msgid) {
    case IDUM_ENTERHALL: {
        UM_CAST(UM_ENTERHALL, enter, msg);
        login(s, source, enter->uid);
        break;
        }
    case IDUM_LOGOUT: {
        logout(s, pr);
        break;
        }
    case IDUM_CHARCREATE: {
        UM_CASTCK(UM_CHARCREATE, create, msg, sz);
        create->name[sizeof(create->name)-1] = '\0';
        create_char(s, pr, create->name);
        break;
        }
    }
}
