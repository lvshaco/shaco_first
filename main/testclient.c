#include "tplt_include.h"
#include "tplt_struct.h"

static struct tplt*
_loadtplt() {
#define TBLFILE(name) "./res/tbl/"#name".tbl"
    struct tplt_desc desc[] = {
        { TPLT_ROLE, sizeof(struct role_tplt), TBLFILE(role), TPLT_VIST_VEC32},
        { TPLT_ITEM, sizeof(struct item_tplt), TBLFILE(item), TPLT_VIST_VEC32},
    };
    return tplt_init(desc, sizeof(desc)/sizeof(desc[0]));
}

int main() {
    _loadtplt();
    return 0;
}
