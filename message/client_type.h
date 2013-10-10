#ifndef __client_type_h__
#define __client_type_h__

#include "host_node.h"

#define CLI_TRUST 0
#define CLI_UNTRUST (int)(HNODE_TID_MAX)+1
#define CLI_CMD     CLI_UNTRUST+1
#define CLI_GAME    CLI_UNTRUST+2

#endif
