#include "sh.h"
#include "elog_include.h"
#include <pthread.h>
#include <unistd.h>

struct log_entry {
    void *msg;
    int sz;
    struct log_entry *next;
};

struct log_tailq {
    struct log_entry *head;
    struct log_entry *tail;
    uint32_t npush;
    uint32_t npop;
    pthread_mutex_t mutex;
};

struct gamelog {
    volatile bool worker_run;
    bool worker_ok;
    pthread_t worker_id; 
    struct log_tailq log_queue;
    struct elog *logger; 
    uint64_t last_time;
    int count;
};

// log
static int
log_push(struct log_tailq *q, void *msg, int sz) {
    struct log_entry *log = malloc(sizeof(*log));
    log->msg = msg;
    log->sz = sz;
    log->next = NULL;
   
    pthread_mutex_lock(&q->mutex);
    if (q->head) {
        //assert(q->tail != NULL);
        //assert(q->tail->next == NULL);
        q->tail->next = log;
    } else {
        q->head = log;
    }
    q->tail = log; 
    pthread_mutex_unlock(&q->mutex);
    q->npush++;
    return 0;
}

static struct log_entry *
log_pop(struct log_tailq *q) {
    pthread_mutex_lock(&q->mutex);
    if (q->head) {
        struct log_entry *log = q->head;
        q->head = log->next;
        pthread_mutex_unlock(&q->mutex);
        q->npop++;
        return log;
    } else {
        pthread_mutex_unlock(&q->mutex);
        return NULL;
    }
    
}

static int 
create_log(struct gamelog *self) {
    struct elog *logger = elog_create("/tmp/gamelog.log");
    if (logger == NULL) {
        return 1;
    }
    if (elog_set_appender(logger, &g_elog_appender_file)) {
        sh_error("gamelog set appender fail");
        return 1;
    }
    self->logger = logger;
    return 0;
}

static uint64_t
_elapsed() {
    struct timespec ti;
    clock_gettime(CLOCK_MONOTONIC, &ti);
    return ti.tv_sec * 1000 + ti.tv_nsec / 1000000;
}

static inline void
log_stat(struct gamelog *self) {
    self->count++;
    if (self->last_time == 0) {
        self->last_time = _elapsed();
    }
    uint64_t now = _elapsed();
    uint64_t diff = now - self->last_time;
    if (diff >= 1000) {
        int rate = self->count / (diff/1000.0);
        self->last_time = now;
        self->count = 0;
        fprintf(stderr, "rate: %d\n", rate);
    }
}

static void *
log_run(void *arg) {
    struct gamelog *self = arg;
    for (;;) {
        struct log_entry *log = log_pop(&self->log_queue);
        if (log) {
            elog_append(self->logger, log->msg, log->sz);
            log_stat(self);
            free(log->msg);
            free(log);
        } else {
            if (self->worker_run) {
                //fprintf(stderr, "sleep ... ");
                usleep(1000);
            } else {
                break;
            }
        }
    }
    return NULL;
}

static int
create_logger(struct gamelog *self) {
    if (create_log(self)) {
        return 1;
    }
    //self->log_queue.mutex = PTHREAD_MUTEX_INITIALIZER;
    memset(&self->log_queue.mutex, 0, sizeof(self->log_queue.mutex));
    self->worker_run = true;

    pthread_t tid;
    int result = pthread_create(&tid, NULL, log_run, self);
    if (result) {
        sh_error("Create log thread(err#%d)", result);
        return 1;
    }
    self->worker_ok = true;
    self->worker_id = tid;
    sh_info("Create log thread(%lu)", self->worker_id);
    return 0;
}

static void
destroy_logger(struct gamelog *self) {
    if (self->worker_ok) {
        self->worker_run = false;
        pthread_join(self->worker_id, NULL);
        self->worker_ok = false;
        fprintf(stderr, "npush %u, npop %u\n", 
                self->log_queue.npush, 
                self->log_queue.npop);
    }
    if (self->logger) {
        elog_free(self->logger);
        self->logger = NULL;
    }
}

// gamelog
struct gamelog *
gamelog_create() {
    struct gamelog *self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
gamelog_free(struct gamelog *self) {
    if (self == NULL)
        return;
    destroy_logger(self);    
    free(self);
}

int
gamelog_init(struct module *s) {
    struct gamelog *self = MODULE_SELF;
    if (sh_handle_publish(MODULE_NAME, PUB_SER)) {
        return 1;
    } 
    if (create_logger(self)) {
        return 1;
    }
    return 0;
}

void
gamelog_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    struct gamelog *self = MODULE_SELF;
    assert(type == MT_TEXT);
   
    char* log = malloc(sz);
    memcpy(log, msg, sz);
    log_push(&self->log_queue, log, sz);
}
