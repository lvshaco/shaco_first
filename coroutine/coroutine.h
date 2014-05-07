#ifndef __coroutine_h__
#define __coroutine_h__

struct coroutine;
struct coroutine *coroutine_create(void (*fn)(void *), int stack_size);
void * coroutine_yield(void *udata);
void * coroutine_resume(struct coroutine *co, void *udata);
const char * coroutine_status(struct coroutine *co);
struct coroutine *coroutine_running();

#endif
