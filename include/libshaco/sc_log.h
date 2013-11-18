#ifndef __sc_log_h__
#define __sc_log_h__

#define LOG_DEBUG   0
#define LOG_INFO    1
#define LOG_WARNING 2
#define LOG_ERROR   3
#define LOG_EXIT    4
#define LOG_PANIC   5
#define LOG_MAX     6 

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

void sc_debug(const char* fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
;

void sc_log_backtrace();

#endif
