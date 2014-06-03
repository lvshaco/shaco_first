-- shaco config base
-- only string value will be read
-- the key prefix by "_" will no be read

local iip = "127.0.0.1"
local oip = "192.168.1.140"
--local oip = "192.168.1.145"
--local wip = "116.228.135.50"
local wip=oip
web_addr = "192.168.1.145"
log_dir = "/home/lvxiaojun/log"
--log_dir = "/home/game/log"
local hb = 30

node_map = {
--
center   = {ip=iip, port=8000, conn=256},
gateload = {ip=iip, port=8001, conn=256},
uniqueol = {ip=iip, port=8002, conn=256},
match    = {ip=iip, port=8003, conn=256},
robot    = {ip=iip, port=8004, conn=256},
bug      = {ip=iip, port=8005, conn=256},
gamelog  = {ip=iip, port=8006, conn=256},
stat     = {ip=iip, port=8007, conn=256},
keepalived={ip=iip, port=8621, conn=256},
--
route    = {ip=iip, port=8100, conn=256},
watchdog1 = {ip=iip, port=8130, conn=256},
watchdog2 = {ip=iip, port=8131, conn=256},
auth     = {ip=iip, port=8160, conn=256},
--
gate     = {ip=iip, port=8200, conn=256},
gate2     = {ip=iip, port=8201, conn=256},
hall     = {ip=iip, port=8300, conn=256},
room     = {ip=iip, port=8400, conn=256},
--
}

open_node_map = {
center = {ip=iip, port=18000, handler="cmds", clientmax=100, clientlive=6, wbuffer=0, verify=0},
route  = {ip=oip, port=18100, handler="route", clientmax=10000, clientlive=0, wbuffer=0},
gate   = {ip=oip, port=18200, handler="watchdog1",clientmax=10000, clientlive=hb, wbuffer=128*1024, load="gateload", wip=wip},
gate2   = {ip=oip, port=18201, handler="watchdog2",clientmax=10000, clientlive=hb, wbuffer=128*1024, load="gateload", wip=wip},
}

function def_node(name, id)
    local node = node_map[name]
    node_id   = id
    node_ip   = node.ip
    node_port = node.port

    sh_loglevel = "TRACE"
    sh_connmax = node.conn
    sh_module = "log,node"
    if name == "center" then
        sh_module = sh_module .. ",centers,cmdctl,cmds"
        keepalive_always = 1
    else
        local center = node_map["center"]
        center_ip   = center.ip
        center_port = center.port
        sh_module = sh_module .. ",cmdctl"
    end
    if name == "keepalived" then
        sh_module = sh_module .. ",keepalived"
    else
        sh_module = sh_module .. ",keepalivec"
    end
    keepalive_ip = "127.0.0.1"
    keepalive_port = 8422

    local open = open_node_map[name]
    if open then
        gate_ip = open.ip
        gate_port = open.port
        wan_ip = open.wip
        gate_wbuffermax = open.wbuffer
        gate_clientmax  = open.clientmax
        gate_clientlive = open.clientlive
        gate_publish = 1
        gate_load  = open.load
        gate_need_verify = open.verify
        gate_handler = open.handler

        sh_connmax = sh_connmax + gate_clientmax
        --sh_module = sh_module .. ",gate"
    end
end
