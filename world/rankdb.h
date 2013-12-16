#ifndef __rankdb_h__
#define __rankdb_h__

#include <stdint.h>

struct rankdbcmd {
    const char* type;
    uint32_t charid;
    uint64_t score;
};

#endif
