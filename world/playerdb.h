#ifndef __playerdb_h__
#define __playerdb_h__

#include <stdint.h>

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
struct playerdbcmd {
    int8_t type;
    struct player* p;
    int8_t err;
};

struct playerdbres {
    struct player* p;
    int error;
};

#endif
