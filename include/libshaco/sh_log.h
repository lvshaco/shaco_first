#ifndef __sh_log_h__
#define __sh_log_h__

#define LOG_DEBUG   0
#define LOG_TRACE   1
#define LOG_INFO    2
#define LOG_WARNING 3
#define LOG_ERROR   4
#define LOG_REC     5
#define LOG_EXIT    6
#define LOG_PANIC   7
#define LOG_MAX     8

int sh_log_level();

const char* sh_log_levelstr(int level);
int  sh_log_setlevelstr(const char* level);

void sh_error(const char* fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
;

void sh_warning(const char* fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
;

void sh_info(const char* fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
;

void sh_trace(const char* fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
;

void sh_debug(const char* fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
;

void sh_rec(const char* fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
;

void sh_log_backtrace();

#endif
