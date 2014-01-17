#include "player.h"
#include "hall.h"
#include "playerdb.h"
#include "user_message.h"
#include "cli_message.h"
#include "sh_util.h"
#include <assert.h>

static inline void
free_player(struct hall *self, struct player *pr) {
    sh_hash_remove(&self->acc2player, pr->data.accid);
    free(pr);
}

int 
player_init(struct hall *self) {
    sh_hash_init(&self->acc2player, 1);
    return 1;
}

void
player_fini(struct hall *self) {
    sh_hash_fini(&self->acc2player);
}

static void 
login(struct service *s, int source, uint32_t accid) {
    struct hall *self = SERVICE_SELF;
    struct player *pr = sh_hash_find(&self->acc2player, accid);
    if (pr) {
        return; // relogin
    }
    pr = malloc(sizeof(*pr));
    memset(pr, 0, sizeof(pr));
    pr->watchdog_source = source;
    pr->status = PS_QUERYCHAR;
    pr->data.accid = accid;
    assert(!sh_hash_insert(&self->acc2player, accid, pr));

    if (playerdb_send(s, pr, PDB_QUERY)) {
        hall_notify_logout(s, pr, SERR_NODB);
        free_player(self, pr);
    }
}

static void 
logout(struct service *s, struct player *pr) {
    struct hall *self = SERVICE_SELF;
 
    // todo notify match
    free_player(self, pr);
}

static void 
create_char(struct service *s, struct player *pr, const char *name) {
    struct hall *self = SERVICE_SELF;
    if (pr->status == PS_WAITCREATECHAR) {
        sc_strncpy(pr->data.name, name, sizeof(pr->data.name));
        if (playerdb_send(s, pr, PDB_CHECKNAME)) {
            hall_notify_logout(s, pr, SERR_NODB);
            free_player(self, pr);
        }
    }
}

void 
player_main(struct service *s, int source, struct player *pr, const void *msg, int sz) {
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
