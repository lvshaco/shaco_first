#ifndef __sc_assert_h__
#define __sc_assert_h__

#include <assert.h>

#define hassertlog(x) if (!(x)) { \
    sc_error("hassert:%s:%d:%s %s", __FILE__, __LINE__, __FUNCTION__, #x); \
}

#endif
