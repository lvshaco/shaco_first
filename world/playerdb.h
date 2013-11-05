#ifndef __playerdb_h__
#define __playerdb_h__

#include <stdint.h>

#define PDB_QUERY 0
#define PDB_LOAD  1
#define PDB_SAVE  2
#define PDB_CHECKNAME 3
#define PDB_CHARID 4
#define PDB_CREATE 5
#define PDB_BINDCHARID 6

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
