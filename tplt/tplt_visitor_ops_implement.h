#ifndef __tplt_visitor_ops_implement_h__
#define __tplt_visitor_ops_implement_h__

#include "tplt_visitor_ops.h"

#if defined(TPLT_BUILD_AS_DLL)
#if defined(TPLT_LIB)
#define TPLT_API __declspec(dllexport)
#else
#define TPLT_API __declspec(dllimport)
#endif
#else
#define TPLT_API extern
#endif
#define TPLT_VAR TPLT_API

#define TPLT_VIST_VEC32 &g_tplt_visitor_vec32
#define TPLT_VIST_INDEX32 &g_tplt_visitor_index32

TPLT_VAR const struct tplt_visitor_ops g_tplt_visitor_vec32;
TPLT_VAR const struct tplt_visitor_ops g_tplt_visitor_index32;

#endif
