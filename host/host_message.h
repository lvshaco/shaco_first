#ifndef __host_message_h__
#define __host_message_h__

#define HMSGT_HOST 0 // shaco内部提供的消息类型
#define HMSGT_USER 1 // 用户自定义消息类型

struct host_message {
    int8_t type;  // see HMSGT_* define
    uint32_t len; // msg length
    uint8_t msg[0];
};

#endif
