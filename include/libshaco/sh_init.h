#ifndef __sh_init_h__
#define __sh_init_h__

struct sh_library_entry {
    const char* filename;
    int fileline;
    int prio;
    void (*init)();
    void (*fini)();
};

void sh_library_entry_register(struct sh_library_entry* entry);

#define SH_LIBRARY_INIT_PRIO(initfn, finifn, prio) \
    __attribute__((constructor)) \
    void _sh_library_init_##initfn() { \
        struct sh_library_entry entry = { \
            __FILE__, __LINE__, prio, initfn, finifn }; \
        sh_library_entry_register(&entry); \
    }

#define SH_LIBRARY_INIT(initfn, finifn) \
    SH_LIBRARY_INIT_PRIO(initfn, finifn, 100)

#endif
