#ifndef __playerdb_h__
#define __playerdb_h__

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

struct service;
struct hall;
struct player;
struct UM_REDISREPLY;

int playerdb_init(struct hall *self);
void playerdb_fini(struct hall *self);
int playerdb_send(struct service *s, struct player *pr, int type);
void playerdb_process_redis(struct service *s, struct UM_REDISREPLY *rep, int sz);

#endif
