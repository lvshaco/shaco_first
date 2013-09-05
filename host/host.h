#ifndef __host_h__
#define __host_h__

int host_create(const char* file);
void host_free();
void host_start();
void host_stop();

int host_getint(const char* key, int def);
const char* host_getstr(const char* key, const char* def);

#endif
