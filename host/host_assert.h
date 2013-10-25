#ifndef __host_assert_h__
#define __host_assert_h__

#include <assert.h>

#define hassertlog(x) if (!(x)) { \
    host_error("hassert:%s:%d:%s %s", __FILE__, __LINE__, __FUNCTION__, #x); \
}

#endif
