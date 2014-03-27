#ifndef __room_cmdctl_h__
#define __room_cmdctl_h__

#include "cmdctl.h"
#include "room_game.h"
#include "room_tplt.h"

static int
reloadres(struct module *s, struct args *A, struct memrw *rw) {
    if (room_tplt_main(s)) {
        return CTL_FAIL;
    }
    return CTL_OK;
}

static int
nuser(struct module *s, struct args *A, struct memrw *rw) {
    struct room *self = MODULE_SELF;
    uint32_t np = self->players.used;
    uint32_t nr = self->room_games.used;
    int n = snprintf(rw->ptr, RW_SPACE(rw), "%u(nplayer) %u(nroom)", np, nr);
    memrw_pos(rw, n); 
    return CTL_OK;
}

static int
user(struct module *s, struct args *A, struct memrw *rw) {
    struct room *self = MODULE_SELF;
    if (A->argc < 2) {
        return CTL_ARGLESS;
    }
    int n;
    uint32_t accid = strtoul(A->argv[1], NULL, 10);
    struct player *m = sh_hash_find(&self->players, accid);
    if (m) {
        struct room_game *ro = room_member_to_game(m);
        n = snprintf(rw->ptr, RW_SPACE(rw), 
                    "accid(%u) charid(%u) name(%s) watchdog(%04x) "
                    "logined(%d) online(%d) loadok(%d) robot(%d) "
                    "room(%u) type(%d) status(%d)",
                    m->detail.accid, m->detail.charid, m->detail.name, 
                    m->watchdog_source,
                    m->logined, m->online, m->loadok, m->is_robot,
                    ro->id, ro->type, ro->status);
    } else {
        n = snprintf(rw->ptr, RW_SPACE(rw), "none");
    }
    memrw_pos(rw, n);
    return CTL_OK;
}

static inline void
build_member(struct match_member *m, int id) {
    m->is_robot = false;
    m->brief.accid = id;
    m->brief.charid = id;
    m->brief.name[0] = '\0';
    m->brief.level = 1;
    m->brief.role = 10;
    m->brief.state = 10;
    m->brief.oxygen = 600;
    m->brief.body = 500;
    m->brief.quick = 500;
}

static int
test_gamecreate(struct module *s, struct args *A, struct memrw *rw) {
    if (A->argc < 2) {
        return CTL_ARGLESS;
    }
    uint64_t t1 = sh_timer_elapsed_real();
    int times = strtol(A->argv[1], NULL, 10);
    int i;
    for (i=0; i<times; ++i) {
        int id = i+1;
        UM_DEFVAR(UM_CREATEROOM, create);
        create->type = 1;
        create->mapid = 1;
        create->id = id;
        create->max_member = 2;
        create->nmember = 2;
        build_member(&create->members[0], id*2+1);
        build_member(&create->members[1], id*2+2);
        game_main(s, -1, create, UM_CREATEROOM_size(create));
    }
    uint64_t t2 = sh_timer_elapsed_real();
    int n = snprintf(rw->ptr, RW_SPACE(rw), "times: %d, use time %u", times, (uint32_t)(t2-t1));
    memrw_pos(rw, n); 
    return CTL_OK;
}

static int
command(struct module *s, int source, int connid, const char *msg, int len, struct memrw *rw) {
    //struct room *self = MODULE_SELF;
    struct args A;
    args_parsestrl(&A, 0, msg, len);
    if (A.argc == 0) {
        return CTL_ARGLESS;
    }
    const char *cmd = A.argv[0];
    if (!strcmp(cmd, "nuser")) {
        return nuser(s, &A, rw);
    } else if (!strcmp(cmd, "user")) {
        return user(s, &A, rw);
    } else if (!strcmp(cmd, "reloadres"))  {
        return reloadres(s, &A, rw);
    } else if (!strcmp(cmd, "test_gamecreate")) {
        return test_gamecreate(s, &A, rw);
    } else {
        return CTL_NOCMD;
    }
}

#endif
