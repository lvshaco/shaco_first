#include "host_service.h"
#include "host_log.h"
#include <stdio.h>

static const char* STR_LEVELS[LOG_MAX] = {
    "DEBUG", "INFO", "WARNING", "ERROR",
};

static const char*
_strlevel(int level) {
    if (level >= 0 && level < LOG_MAX)
        return STR_LEVELS[level];
    return "";
}

static int
_idlevel(const char* level) {
    int i;
    for (i=0; i<LOG_MAX; ++i) {
        if (strcmp(STR_LEVELS[i], level) == 0)
            return i;
    }
    return LOG_DEBUG;
}

struct _log {
    int level;
};

struct _log*
log_create() {
    struct _log* self = malloc(sizeof(*self));
    self->level = LOG_DEBUG;
    return self;
}

void
log_free(struct _log* self) {
    free(self);
}

int
log_init(struct service* s) {
    struct _log* self = s->content;
    // todo
    self->level = _idlevel("DEBUG");
    return 0;
}

void
log_service(struct service* s, struct service_message* sm) {
    struct _log* self = s->content;
    int level = sm->sessionid;
   
    if (level >= self->level) {
        printf("%s:%s\n", _strlevel(level), sm->msg);
    }
}
