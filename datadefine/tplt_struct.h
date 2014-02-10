#ifndef __tplt_struct_h__
#define __tplt_struct_h__

#include <stdint.h>

#define TPLT_RING 0
#define TPLT_STONE 1
#define TPLT_EXP 2
#define TPLT_ROLE 3
#define TPLT_ITEM 4
#define TPLT_MAP 5

#pragma pack(1)

// 戒指
struct ring_tplt {
    uint32_t id;             // ID
    uint32_t type;           // 类型
    char name[32];           // 名称
    char desc[96];           // 简介
    uint32_t level;          // 等级
    uint32_t weight;         // 权值
    uint32_t saleprice;      // 出售价格
    int32_t effect1;         // 效果1
    int32_t valuet1;         // 值类型1
    int32_t value1;          // 值1
    int32_t effect2;         // 效果2
    int32_t valuet2;         // 值类型2
    int32_t value2;          // 值2
    int32_t effect3;         // 效果3
    int32_t valuet3;         // 值类型3
    int32_t value3;          // 值3

};

// 秘石
struct stone_tplt {
    uint32_t id;             // ID
    uint32_t weight;         // 权值

};

// 经验
struct exp_tplt {
    uint32_t level;          // 等级ID
    uint32_t curexp;         // 当前等级所需经验值

};

// 角色数据
struct role_tplt {
    uint32_t id;             // ID
    char name[32];           // 角色名称
    uint32_t sex;            // 性别
    int32_t oxygen;          // 氧气
    int32_t body;            // 体能
    int32_t quick;           // 敏捷
    int32_t coin_profit;     // 金币收益
    int32_t lucky;           // 幸运
    int32_t attack_distance; // 挖掘距离
    int32_t attack_range;    // 挖掘范围
    int32_t attack_speed;    // 挖掘速度
    int32_t jump_range;      // 跳跃高度
    int32_t sence_range;     // 感知
    int32_t view_range;      // 视野
    int32_t attack_power;    // 攻击力
    uint32_t needlevel;      // 所需等级
    uint32_t needcoin;       // 所需金币
    uint32_t needdiamond;    // 所需钻石
    int32_t effect1;         // 效果类型1
    int32_t valuet1;         // 效果值类型1
    int32_t value1;          // 效果值1
    int32_t effect2;         // 效果类型2
    int32_t valuet2;         // 效果值类型2
    int32_t value2;          // 效果值2
    int32_t effect3;         // 效果类型3
    int32_t valuet3;         // 效果值类型3
    int32_t value3;          // 效果值3
    int32_t effect4;         // 效果类型4
    int32_t valuet4;         // 效果值类型4
    int32_t value4;          // 效果值4
    int32_t effect5;         // 效果类型5
    int32_t valuet5;         // 效果值类型5
    int32_t value5;          // 效果值5

};

// 道具
struct item_tplt {
    uint32_t id;             // ID
    uint32_t type;           // 类型
    uint32_t subtype;        // 子类
    uint32_t luck;           // 运气
    int32_t target;          // 目标
    int32_t uptime;          // 漂浮时间
    int32_t delay;           // 延迟时间
    int32_t time;            // 持续时间
    int32_t effect1;         // 效果类型1
    int32_t valuet1;         // 效果值类型1
    int32_t value1;          // 效果值1
    int32_t effect2;         // 效果类型2
    int32_t valuet2;         // 效果值类型2
    int32_t value2;          // 效果值2
    int32_t effect3;         // 效果类型3
    int32_t valuet3;         // 效果值类型3
    int32_t value3;          // 效果值3

};

// 关卡管理
struct map_tplt {
    uint32_t id;             // ID
    uint32_t difficulty;     // 难度
    uint32_t width;          // 宽度
    uint32_t height;         // 深度
    uint32_t block;          // 地块配置
    uint16_t ncolortex;
    uint32_t colortex[10]; // 地块纹理
    uint16_t nspectex;
    uint32_t spectex[5]; // 特殊纹理
    uint16_t nfightitem;
    uint32_t fightitem[10]; // 道具配置
    uint16_t ntrapitem;
    uint32_t trapitem[10]; // 机关配置
    uint16_t nbaoitem1;
    uint32_t baoitem1[10]; // 秘石1配置
    uint16_t nbaoitem2;
    uint32_t baoitem2[10]; // 秘石2配置
    uint16_t nbaoitem3;
    uint32_t baoitem3[10]; // 秘石3配置
    uint16_t nbaoitem4;
    uint32_t baoitem4[10]; // 秘石4配置
    uint16_t nbaoitem5;
    uint32_t baoitem5[10]; // 秘石5配置
    uint16_t nbaoitem6;
    uint32_t baoitem6[10]; // 秘石6配置
    uint16_t nbaoitem7;
    uint32_t baoitem7[10]; // 秘石7配置
    uint16_t nbaoitem8;
    uint32_t baoitem8[10]; // 秘石8配置
    uint16_t nbaoitem9;
    uint32_t baoitem9[10]; // 秘石9配置
    uint16_t nbaoitem10;
    uint32_t baoitem10[10]; // 秘石10配置

};

#pragma pack()

#endif
