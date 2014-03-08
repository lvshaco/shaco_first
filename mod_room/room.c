#include "room.h"

int
room_online_nplayer(struct room_game *ro) {
    int i, n=0;
    for (i=0; i<ro->np; ++i) {
        struct player *m = &ro->p[i];
        if (is_player(m) && 
            is_online(m))
            n++;
    }
    return n;
}

static struct member_n
room_preonline_n(struct room_game *ro) {
    struct member_n n = {0,0};
    int i;
    for (i=0; i<ro->np; ++i) {
        struct player *m = &ro->p[i];
        if (is_online(m) || !is_logined(m)) {
            if (is_player(m))
                n.player++;
            else
                n.robot++;
        }
    }
    return n;
}

bool
room_preonline_1player(struct room_game *ro) {
    struct member_n n = room_preonline_n(ro);
    return (n.player == 1) && (n.robot == 0);
}
