#ifndef __tplt_internal_h__
#define __tplt_internal_h__

#ifdef USE_HOSTLOG
#include "sc_log.h"
#define TPLT_LOGERR sc_error
#define TPLT_LOGINFO sc_info
#endif
#ifndef TPLT_LOGERR
#include <stdio.h>
#define TPLT_LOGERR printf
#endif
#ifndef TPLT_LOGINFO
#include <stdio.h>
#define TPLT_LOGINFO printf
#endif

#endif
