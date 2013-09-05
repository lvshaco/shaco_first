#ifndef __user_message_h__
#define __user_message_h__

#include "host_net.h"
#include <stdint.h>
#include <stdlib.h>

#define user_message_header \
    uint16_t size;

struct user_message {
    user_message_header;
    uint8_t data[0];
};

static struct user_message*
user_message_read(int id, int* error) {
    struct user_message* h = host_net_read(id, sizeof(*h));
    if (h == NULL) {
        *error = host_net_error();
        return NULL;
    }
    
    void* data = host_net_read(id, h->size);
    if (data == NULL) {
        *error = host_net_error();
        return NULL;
    }
    return h;
}

#endif
