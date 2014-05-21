#include "sh.h"
#include "elog_include.h"
#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>

// todo: add drop msg if log_queue accumulate too much msg, 
// note: msg accumulate size must be volatile

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
    time_t next_logger_time; 
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
create_log(struct module *s, time_t now) {
    struct gamelog *self = MODULE_SELF;
    
    if (self->logger != NULL) {
        elog_free(self->logger);
        self->logger = NULL;
    }
    char field_d[64];
    char field_t[64];
    sh_snprintf(field_d, sizeof(field_d), "%s_dir",  MODULE_NAME); 
    sh_snprintf(field_t, sizeof(field_t), "%s_type", MODULE_NAME);

    struct tm tmnow = *localtime(&now);
    time_t base = sh_day_base(now, tmnow);
    self->next_logger_time = base + SH_DAY_SECS;

    char fname[PATH_MAX];
    sh_snprintf(fname, sizeof(fname), "%s/%s_%04u%02u%02u.gamelog", 
            sh_getstr(field_d, ""), 
            sh_getstr(field_t, ""),
            tmnow.tm_year+1900, tmnow.tm_mon+1, tmnow.tm_mday);
    struct elog *logger = elog_create(fname);
    if (logger == NULL) {
        return 1;
    }
    if (elog_set_appender(logger, &g_elog_appender_file, "a+")) {
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
    struct module *s = arg;
    struct gamelog *self = MODULE_SELF;
    while (self->worker_run) {
        struct log_data log;
        int result = log_queue_pop(&self->log_q, &log);
        if (result == 0) { 
            time_t now = time(NULL);
            if (now >= self->next_logger_time) {
                create_log(s, now);
            }
            elog_append(self->logger, log.msg, log.sz);
            free(log.msg);
            log_stat(self);
        } else {
            usleep(10);
        }
    }
    return NULL;
}

static int
create_logger(struct module *s) {
    struct gamelog *self = MODULE_SELF;
   
    char field_d[64];
    sh_snprintf(field_d, sizeof(field_d), "%s_dir", MODULE_NAME); 
    const char *dir = sh_getstr(field_d, "");
    if (dir[0] == '\0') {
        sh_exit("`%s` no specify gamelog dir", MODULE_NAME);
        return 1;
    }
    mkdir(dir, 0744);

    time_t now = time(NULL);
    if (create_log(s, now)) {
        return 1;
    }
    log_queue_init(&self->log_q);
    
    self->worker_run = true;
    pthread_t tid;
    int result = pthread_create(&tid, NULL, log_run, s);
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
    if (sh_handle_publish(MODULE_NAME, PUB_SER)) {
        return 1;
    }  
    if (create_logger(s)) {
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
