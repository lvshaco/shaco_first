#ifndef __client_type_h__
#define __client_type_h__

#include "sc_node.h"

// must be > 0, see NETUT_TRUST
#define CLI_UNTRUST (int)(HNODE_TID_MAX)+1
#define CLI_CMD     CLI_UNTRUST+1
#define CLI_GAME    CLI_UNTRUST+2
#define CLI_REDIS   CLI_UNTRUST+3

#endif
