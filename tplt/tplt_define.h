#ifndef __tplt_define_h__
#define __tplt_define_h__

#include <stdint.h>

#define TPLT_SKINDATA 0

#pragma pack(1)

// 角色数据
struct skindata_tplt {
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

#pragma pack()

#endif