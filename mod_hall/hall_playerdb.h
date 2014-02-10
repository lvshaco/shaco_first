#ifndef __hall_playerdb_h__
#define __hall_playerdb_h__

#include <stdint.h>
#include <stddef.h>

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

struct module;
struct hall;
struct player;
struct UM_REDISREPLY;

int hall_playerdb_init(struct hall *self);
void hall_playerdb_fini(struct hall *self);
int hall_playerdb_send(struct module *s, struct player *pr, int type);
void hall_playerdb_process_redis(struct module *s, struct UM_REDISREPLY *rep, int sz);

#endif
