#ifndef __sc_env_h__
#define __sc_env_h__

void sc_env_init();
void sc_env_fini();

const char* sc_getenv(const char* key);
const char* sc_getstr(const char* key, const char* def);
float sc_getnum(const char* key, float def);
#define sc_getint (int)sc_getnum

void sc_setenv(const char* key, const char* value);
void sc_setnumenv(const char* key, float value);

#endif
