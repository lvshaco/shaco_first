extern "C" {
#include "sh.h"
#include "cmdctl.h"
#include <stdlib.h>
#include "msg.h"
#include "msg_server.h"
#include "msg_client.h"
}

#include "test.h"

struct test {
};

extern "C" {

struct test *
test_create() {
    return NULL;
}

void
test_free(struct test *self) {
}

int
test_init(struct module *s) {
    //struct test *self = (struct test*)MODULE_SELF;
    Test t;
    sh_error("%d", t.get());
    return 0;
}

void
test_time(struct module *s) {
    //struct test *self = MODULE_SELF;
}

void
test_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    //struct test *self = MODULE_SELF;
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        }
        break;
        }
    case MT_CMD:
        cmdctl(s, source, msg, sz, NULL);
        break;
    }

}

}
