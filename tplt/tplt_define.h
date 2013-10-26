#ifndef __tplt_define_h__
#define __tplt_define_h__

#include <stdint.h>

#define TPLT_COPYMAP 0

#pragma pack(1)

// copymap_data
struct copymap_tplt {
    uint32_t id;             // 副本编号
    char name[32];           // 副本名称
    uint32_t chaodai;        // 朝代编号
    uint32_t icon_id;        // 图标ID
    int32_t icon_posx;       // 图标位置X
    int32_t icon_posy;       // 图标位置Y
    uint64_t npc_icon;       // NPC图标
    uint32_t man_count;      // 推荐人数
    char desc[128];          // 简介

};

#pragma pack()

#endif