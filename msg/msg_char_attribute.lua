local strfmt = string.format

local char_attribute = {
{ field="oxygen", type="int32_t", client=true, other=true, comment= "氧气"},
{ field="body", type="int32_t", client=true, other=false, comment= "体能"},
{ field="quick", type="int32_t", client=true, other=false, comment= "敏捷"},
    
{ field="movespeed", type="float", client=true, other=false, comment= "移动速度"},
{ field="movespeedadd", type="float", client=true, other=false, comment= "移动速度add"},
{ field="charfallspeed", type="float", client=true, other=false, comment= "坠落速度"},
{ field="charfallspeedadd", type="float", client=true, other=false, comment= "坠落速度add"},
{ field="jmpspeed", type="float", client=true, other=false, comment= "跳跃速度--"},
{ field="jmpacctime", type="int32_t", client=true, other=false, comment= "跳跃准备时间--"},
{ field="rebirthtime", type="int32_t", client=true, other=false, comment= "复活时间"},
{ field="rebirthtimeadd", type="float", client=true, other=false, comment= "复活时间add"},
{ field="dodgedistance", type="float", client=true, other=false, comment= "闪避距离"},
{ field="dodgedistanceadd", type="float", client=true, other=false, comment= "闪避距离add"},
{ field="jump_range", type="int32_t", client=true, other=false, comment= "跳跃高度"},
{ field="sence_range", type="int32_t", client=true, other=false, comment= "感知范围"},
{ field="view_range", type="int32_t", client=true, other=false, comment= "视野范围"},
   
{ field="attack_power", type="int32_t", client=true, other=false, comment= "攻击力，攻击一下扣除的地块耐久度"},
{ field="attack_distance", type="int32_t", client=true, other=false, comment= "攻击距离，离地块的最大长度"},
{ field="attack_range", type="int32_t", client=true, other=false, comment= "攻击范围，攻击的格子范围"},
{ field="attack_speed", type="int32_t", client=true, other=false, comment= "攻击速度毫秒"},

{ field="coin_profit", type="float", client=false, other=false, comment= "金币收益"},
{ field="wincoin_profit", type="float", client=false, other=false, comment= "胜利金币收益"},
{ field="score_profit", type="float", client=false, other=false, comment= "得分收益"},
{ field="winscore_profit", type="float", client=false, other=false, comment= "胜利得分收益"},
{ field="exp_profit", type="float", client=false, other=false, comment= "经验收益"},
{ field="item_timeadd", type="float", client=false, other=false, comment= "物品时长加成"},
{ field="item_oxygenadd", type="float", client=false, other=false, comment= "物品氧气效果加成"},
{ field="lucky", type="int32_t", client=false, other=false, comment= "幸运"},
{ field="prices", type="int32_t", client=false, other=false, comment= "身价"},
{ field="effect_states", type="int32_t", client=true, other=false, comment= "效果状态"},
};
local N = #char_attribute
local FLAG_MAX=8
assert((N+7)/8 <= FLAG_MAX)

local fname = "msg_char_attribute"

local f = io.open(strfmt("%s.h", fname), "w")

local function wri(s)
    f:write(s)
end
local function tab(s)
    f:write("    ")
    f:write(s)
end
local function line()
    f:write("\n")
end

wri(strfmt("#ifndef __%s_h__\n", fname))
wri(strfmt("#define __%s_h__\n", fname))
line()
wri(strfmt("#define FLAG_MAX %d\n", FLAG_MAX))
line()

-- struct char_attribute
wri("struct char_attribute {\n")
for _, v in ipairs(char_attribute) do
    wri(strfmt("    %s %s; // %s\n", v.type, v.field, v.comment))
end
wri("};\n")
line()

-- define char_attri_
for i, v in ipairs(char_attribute) do
    wri(strfmt("#define char_attri_%s %d\n", v.field, i-1))
end
line()

wri("#define char_attri_bit(t) (1<<char_attri_##t%8)\n")
line()

local function pad()
    for i=(N+7)/8, FLAG_MAX do
        tab(strfmt("0, // flag %d\n", i))
    end
end
wri("#endif")
f:close()

f = io.open("msg_char_attribute_up.h", "w")

-- char_attri_client
wri("static uint8_t char_attri_client[FLAG_MAX] = {\n")
for i, v in ipairs(char_attribute) do
    if i%8==1 then
        tab("0")
    end
    if v.client then
        wri("|\n")
        tab(strfmt("char_attri_bit(%s)", v.field))
    end
    if i%8 == 0 or i==N then
        wri(strfmt(", // flag %d\n", (i+7)/8-1))
    end
end
pad()
wri("};\n")

-- char_attri_other
wri("static uint8_t char_attri_other[FLAG_MAX] = {\n")
for i, v in ipairs(char_attribute) do
    if i%8==1 then
        tab("0")
    end
    if v.other then
        wri("|\n")
        tab(strfmt("char_attri_bit(%s)", v.field))
    end
    if i%8 == 0 or i==N then
        wri(strfmt(", // flag %d\n", (i+7)/8-1))
    end
end
pad()
wri("};\n")
f:close()
