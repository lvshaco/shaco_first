#ifndef __tplt_internal_h__
#define __tplt_internal_h__

#ifdef USE_HOSTLOG
#include "sh_log.h"
#define TPLT_LOGERR sh_error
#define TPLT_LOGINFO sh_info
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
