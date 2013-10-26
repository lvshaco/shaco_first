#ifndef __tplt_h__
#define __tplt_h__

struct tplt_desc {
    int type; // see TPLT_*
    int size; // sizeof(*_tplt)
    const char* name;
};

int tplt_init(const struct tplt_desc* desc, int sz);
void tplt_fini();

#endif
