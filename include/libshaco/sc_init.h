#ifndef __sc_init_h__
#define __sc_init_h__

struct sc_library_entry {
    const char* filename;
    int fileline;
    int prio;
    void (*init)();
    void (*fini)();
};

void sc_library_entry_register(struct sc_library_entry* entry);

#define SC_LIBRARY_INIT_PRIO(initfn, finifn, prio) \
    __attribute__((constructor)) \
    void _sc_library_init_##initfn() { \
        struct sc_library_entry entry = { \
            __FILE__, __LINE__, prio, initfn, finifn }; \
        sc_library_entry_register(&entry); \
    }

#define SC_LIBRARY_INIT(initfn, finifn) \
    SC_LIBRARY_INIT_PRIO(initfn, finifn, 100)

#endif
