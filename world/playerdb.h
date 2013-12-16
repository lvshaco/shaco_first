#ifndef __playerdb_h__
#define __playerdb_h__

#include <stdint.h>
#include <stddef.h>

// DB type
#define DB_PLAYER 0
#define DB_OFFLINE 1

// DB_PLAYER type
#define PDB_UNKNOW 0
#define PDB_QUERY 1
#define PDB_LOAD  2
#define PDB_SAVE  3
#define PDB_CHECKNAME 4
#define PDB_SAVENAME 5
#define PDB_CHARID 6
#define PDB_CREATE 7
#define PDB_BINDCHARID 8

struct player;

static inline int
send_playerdb(int dbhandler, struct player* p, int8_t type) {
    struct service_message sm = {0, DB_PLAYER, type, 0, p, 0};
    service_notify_service(dbhandler, &sm); 
    return (int)(ptrdiff_t)sm.result;
}

static inline int
send_offlinedb(int dbhandler, char* sql, int sz) {
    struct service_message sm = { 0, DB_OFFLINE, 0, sz, sql, 0};
    service_notify_service(dbhandler, &sm);
    return (int)(ptrdiff_t)sm.result;
}

#endif
