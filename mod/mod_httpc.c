#include "sh.h"
#include "cmdctl.h"
#include "http_parser.h"
#include <stdlib.h>

static int I = 0;
static int I2 = 0;
int on_message_begin(http_parser* _) {
  (void)_;
  printf("\n***MESSAGE BEGIN(%d)***\n\n", ++I);
  return 0;
}

int on_headers_complete(http_parser* _) {
  (void)_;
  printf("\n***HEADERS COMPLETE***\n\n");
  return 0;
}

int on_message_complete(http_parser* _) {
  (void)_;
  printf("\n***MESSAGE COMPLETE(%d)***\n\n", ++I2);
  return 0;
}

int on_url(http_parser* _, const char* at, size_t length) {
  (void)_;
  printf("Url: %.*s\n", (int)length, at);
  return 0;
}

int on_header_field(http_parser* _, const char* at, size_t length) {
  (void)_;
  printf("Header field: %.*s\n", (int)length, at);
  return 0;
}

int on_header_value(http_parser* _, const char* at, size_t length) {
  (void)_;
  printf("Header value: %.*s\n", (int)length, at);
  return 0;
}

int on_body(http_parser* _, const char* at, size_t length) {
  (void)_;
  printf("Body: %.*s\n", (int)length, at);
  return 0;
}

// ------------------

struct client {
    char *buf;
};

struct httpc {
    int connid;
    struct http_parser *parser;
    char body[64*1024];
    struct sh_hash clients;
};

struct httpc *
httpc_create() {
    struct httpc *self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
httpc_free(struct httpc *self) {
    if (self == NULL)
        return;
    if (self->parser) {
        free(self->parser);
        self->parser = NULL;
    }
}

static int
connect(struct module *s) {
    struct httpc *self = MODULE_SELF;
    const char *ip = sh_getstr("httpd_ip", "");
    int port = sh_getint("httpd_port", 0);
    int err;
    self->connid = sh_net_block_connect(ip, port, MODULE_ID, 0, &err);
    if (self->connid < 0) {
        sh_info("Connect httpd %s:%u fail: %s", ip, port, sh_net_error(err));
        return 1;
    } else {
        sh_net_subscribe(self->connid, true);
        sh_info("Connect httpd(%d) %s:%u ok", self->connid, ip, port);
        return 0;
    }
}

static int
h_send(struct module *s) {
    struct httpc *self = MODULE_SELF; 
    char str[1024];
    size_t len;
    /*len = sh_snprintf(str, sizeof(str), */
        //"GET /rank?t=dashi HTTP/1.1\r\n"
        //"Host: %s\r\n"
        //"Keep-Alive: 3000\r\n"
        //"Connection: keep-alive\r\n"
        //"Accept: */*\r\n"
        /*"\r\n", sh_getstr("httpd_ip", ""));*/
    len = sh_snprintf(str, sizeof(str), 
        "GET /verifyReceipt\r\n"
        "Host: %s\r\n"
        //"Keep-Alive: 3000\r\n"
        //"Connection: keep-alive\r\n"
        //"Accept: */*\r\n"
        "\r\n", sh_getstr("httpd_ip", ""));
    
    void *data = malloc(len);
    memcpy(data, str, len);
    return sh_net_send(self->connid, data, len);
}

int
httpc_init(struct module *s) {
    struct httpc *self = MODULE_SELF;
    self->connid = -1;
    self->parser = malloc(sizeof(*self->parser)); 
    http_parser_init(self->parser, HTTP_RESPONSE);
    if (connect(s)) {
        return 1;
    }
    int i, n = 0;
    for (i=0; i<10000; ++i, ++n) {
        if (h_send(s))
            break;
    }
    sh_info("----send count %d", n);
    //sh_timer_register(MODULE_ID, 1000);
    return 0;
}

static void
read(struct module *s, struct net_message* nm) {
    struct httpc* self = MODULE_SELF;
    int id = nm->connid;
    int e = 0;

    struct http_parser_settings settings;
    memset(&settings, 0, sizeof(settings));
    settings.on_message_begin = on_message_begin;
    settings.on_url = on_url;
    settings.on_header_field = on_header_field;
    settings.on_header_value = on_header_value;
    settings.on_headers_complete = on_headers_complete;
    settings.on_body = on_body;
    settings.on_message_complete = on_message_complete;

    for (;;) {
        char buf[64*1024];
        int nread = sh_net_readto(id, buf, sizeof(buf), &e);
        if (nread > 0) { 
            size_t n = http_parser_execute(self->parser, &settings, buf, nread);
            if (n != nread) {
                e = 999;
                goto errout;
            }
        } else if (nread < 0) {
            goto errout;
        } else {
            goto out;
        }
    }
out:
    return; 
errout:
    sh_net_close_socket(id, true);
    nm->type = NETE_SOCKERR;
    nm->error = e;
    module_net(nm->ud, nm);
}

void
httpc_net(struct module* s, struct net_message* nm) {
    //struct httpc * self = MODULE_SELF;
    switch (nm->type) {
    case NETE_READ:
        read(s, nm);
        break;
    case NETE_SOCKERR: {
        sh_error("%d disconnect: %s", nm->connid, sh_net_error(nm->error));
        }
        break;
    }
}

void
httpc_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    //struct httpc *self = MODULE_SELF;
    switch (type) {
    case MT_CMD:
        cmdctl(s, source, msg, sz, NULL);
        break;
    }
}
