#ifndef __sh_env_h__
#define __sh_env_h__

void sh_env_init();
void sh_env_fini();

const char* sh_getenv(const char* key);
const char* sh_getstr(const char* key, const char* def);
float sh_getnum(const char* key, float def);
float sh_getnum_inrange(const char *key, float min, float max);
#define sh_getint (int)sh_getnum
#define sh_getint_inrange (int)sh_getnum_inrange
void sh_setenv(const char* key, const char* value);
void sh_setnumenv(const char* key, float value);

#endif
