#include "sh.h"
#include "elog_include.h"
#include <pthread.h>
#include <unistd.h>

// log queue
struct log_data {
    void *msg;
    int sz;
};

struct log_entry {
    struct log_data   data;
    struct log_entry *next;
};

struct log_queue {
    struct log_entry *head;
    struct log_entry *tail;
};

static void
log_queue_init(struct log_queue *q) {
    struct log_entry *log = malloc(sizeof(*log));
    log->next = NULL;

    q->head = log; 
    q->tail = log;
}

static void
log_queue_fini(struct log_queue *q) {
    if (q->head) {
        free(q->head);
        q->head = NULL;
        q->tail = NULL;
    }
}

static int
log_queue_push(struct log_queue *q, const struct log_data *data) {
    struct log_entry *log = malloc(sizeof(*log));
    log->next = NULL;

    q->tail->data = *data;

    __sync_synchronize();

    q->tail->next = log;
    q->tail = log;
    return 0;
}

static int
log_queue_pop(struct log_queue *q, struct log_data *data) {
    struct log_entry *log;
    if (q->head->next != NULL) {
        log = q->head;
        q->head = log->next;
        *data = log->data;
        free(log);
        return 0;
    } else {
        return 1;
    }
}

// gamelog
struct gamelog {
    bool worker_ok;
    pthread_t worker_id; 
    volatile bool worker_run;
    struct log_queue log_q;
    struct elog *logger;
    
    uint64_t log_total_in;
    uint64_t log_last_time;
    uint64_t log_last;
    uint64_t log_start_time;
    uint64_t log_total;
    uint64_t log_avg;
    uint64_t log_max;
    uint64_t log_min;
};

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
    uint64_t now = _elapsed();
    self->log_last++;
    self->log_total++;
    if (self->log_last_time == 0) {
        self->log_last_time  = now;
        self->log_start_time = now;
    }
    
    uint64_t diff = now - self->log_last_time;
    if (diff >= 1000) {
        uint64_t log_avg = self->log_last / (diff/1000.0);
        if (self->log_min > log_avg ||
            self->log_min == 0)
            self->log_min = log_avg;
        if (self->log_max < log_avg)
            self->log_max = log_avg;

        self->log_last = 0;
        self->log_last_time = now;
        uint64_t elapse = now - self->log_start_time;
        self->log_avg = self->log_total / (elapse/1000.0);
    }
}

static void *
log_run(void *arg) {
    struct gamelog *self = arg;
    for (;;) {
        struct log_data log;
        int result = log_queue_pop(&self->log_q, &log);
        if (result == 0) { 
            elog_append(self->logger, log.msg, log.sz);
            free(log.msg);
            log_stat(self);
        } else {
            if (self->worker_run) {
                usleep(10);
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
    log_queue_init(&self->log_q);
    
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
dump_log_stat(struct gamelog *self) {
    sh_info("|*********************************log stat*********************************|");
    sh_info("log_total_in %llu, log_total %llu, log_avg %llu, log_max %llu, log_min %llu",
            (unsigned long long)self->log_total_in,
            (unsigned long long)self->log_total,
            (unsigned long long)self->log_avg,
            (unsigned long long)self->log_max,
            (unsigned long long)self->log_min);
    sh_info("|**************************************************************************|");
}

static void
destroy_logger(struct gamelog *self) {
    if (self->worker_ok) {
        self->worker_run = false;

        sh_warning("Waiting for log thread exit ... ");

        pthread_join(self->worker_id, NULL);
        self->worker_ok = false;
        log_queue_fini(&self->log_q);

        sh_warning("Log thread exit");

        dump_log_stat(self);
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
  
    struct log_data log;
    log.msg = malloc(sz);
    log.sz  = sz;
    memcpy(log.msg, msg, sz);
    log_queue_push(&self->log_q, &log);
    self->log_total_in++;
}
