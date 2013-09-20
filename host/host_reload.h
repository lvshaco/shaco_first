#ifndef __host_reload_h__
#define __host_reload_h__

int  host_reload_init();
void host_reload_fini();
void host_reload_prepare(const char* names);
void host_reload_execute();

#endif
