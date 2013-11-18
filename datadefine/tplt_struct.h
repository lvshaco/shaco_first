#ifndef __tplt_struct_h__
#define __tplt_struct_h__

#include <stdint.h>

#define TPLT_ROLE 0
#define TPLT_ITEM 1

#pragma pack(1)

// 角色数据
struct role_tplt {
    uint32_t id;             // ID
    char name[32];           // 角色名称
    uint32_t sex;            // 性别
    uint32_t oxygen;         // 氧气
    uint32_t body;           // 体能
    uint32_t quick;          // 敏捷
    uint32_t coinprofit;     // 金币收益
    uint32_t lucky;          // 幸运
    uint32_t wadist;         // 挖掘距离
    uint32_t warange;        // 挖掘范围
    uint32_t waspeed;        // 挖掘速度
    uint32_t jump;           // 跳跃
    uint32_t sence;          // 感知
    uint32_t view;           // 视野
    uint32_t weapon;         // 武器强度

};

// 道具
struct item_tplt {
    uint32_t id;             // ID
    int32_t target;          // 目标
    int32_t time;            // 持续时间
    int32_t effect1;         // 效果类型1
    int32_t effectvalue1;    // 效果值1
    int32_t effect2;         // 效果类型2
    int32_t effectvalue2;    // 效果值2
    int32_t effect3;         // 效果类型3
    int32_t effectvalue3;    // 效果值3

};

#pragma pack()

#endif