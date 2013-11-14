#ifndef __host_env_h__
#define __host_env_h__

void host_env_init();
void host_env_fini();

const char* host_getenv(const char* key);
const char* host_getstr(const char* key, const char* def);
float host_getnum(const char* key, float def);
#define host_getint (int)host_getnum

void host_setenv(const char* key, const char* value);
void host_setnumenv(const char* key, float value);

#endif
