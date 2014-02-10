#ifndef __sh_env_h__
#define __sh_env_h__

void sh_env_init();
void sh_env_fini();

const char* sh_getenv(const char* key);
const char* sh_getstr(const char* key, const char* def);
float sh_getnum(const char* key, float def);
#define sh_getint (int)sh_getnum

void sh_setenv(const char* key, const char* value);
void sh_setnumenv(const char* key, float value);

#endif
