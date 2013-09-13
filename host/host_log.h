#ifndef __host_log_h__
#define __host_log_h__

#define LOG_DEBUG   0
#define LOG_INFO    1
#define LOG_WARNING 2
#define LOG_ERROR   3
#define LOG_MAX     4

int host_log_init();
void host_log_fini();

const char* host_log_levelstr(int level);
int  host_log_levelid(const char* level);
void host_log_setlevel(int level);
void host_log_setlevelstr(const char* level);

void host_error(const char* fmt, ...);
void host_warning(const char* fmt, ...);
void host_info(const char* fmt, ...);
void host_debug(const char* fmt, ...);

#endif
