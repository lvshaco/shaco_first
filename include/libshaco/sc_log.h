#ifndef __sc_log_h__
#define __sc_log_h__

#define LOG_DEBUG   0
#define LOG_TRACE   1
#define LOG_INFO    2
#define LOG_WARNING 3
#define LOG_ERROR   4
#define LOG_REC     5
#define LOG_EXIT    6
#define LOG_PANIC   7
#define LOG_MAX     8

int sc_log_level();

const char* sc_log_levelstr(int level);
int  sc_log_setlevelstr(const char* level);

void sc_error(const char* fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
;

void sc_warning(const char* fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
;

void sc_info(const char* fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
;

void sc_trace(const char* fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
;

void sc_debug(const char* fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
;

void sc_rec(const char* fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
;

void sc_log_backtrace();

#endif
