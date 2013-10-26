#ifndef __tplt_internal_h__
#define __tplt_internal_h__

#ifdef USE_HOSTLOG
#include "host_log.h"
#define TPLT_LOGERR host_error
#define TPLT_LOGINFO host_info
#endif
#ifndef TPLT_LOGERR
#define TPLT_LOGERR printf
#endif
#ifndef TPLT_LOGINFO
#define TPLT_LOGINFO printf
#endif

#endif
