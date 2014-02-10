-- shaco config base
-- only string value will be read
-- the key prefix by "_" will no be read

local iip = "127.0.0.1"
local oip = "192.168.1.140"
web_addr = "192.168.1.145"
log_dir = "/home/lvxiaojun/log"
local hb = 60

node_map = {
--
center   = {ip=iip, port=8000, conn=256},
gateload = {ip=iip, port=8001, conn=256},
uniqueol = {ip=iip, port=8002, conn=256},
match    = {ip=iip, port=8003, conn=256},
robot    = {ip=iip, port=8004, conn=256},
--
route    = {ip=iip, port=8100, conn=256},
watchdog = {ip=iip, port=8130, conn=256},
auth     = {ip=iip, port=8160, conn=256},
--
gate     = {ip=iip, port=8200, conn=256},
hall     = {ip=iip, port=8300, conn=256},
room     = {ip=iip, port=8400, conn=256},
--
rpacc    = {ip=iip, port=8500, conn=256},
rpuser   = {ip=iip, port=8501, conn=256},
rprank   = {ip=iip, port=8502, conn=256},
}

open_node_map = {
center = {ip=iip, port=18000, handler="cmds", clientmax=100, clientlive=6, wbuffer=0, verify=0},
route  = {ip=oip, port=18100, handler="route", clientmax=10000, clientlive=0, wbuffer=0},
gate   = {ip=oip, port=18200, handler="watchdog",clientmax=10000, clientlive=hb, wbuffer=128*1024, load="gate_load"},
}

function def_node(name, id)
    local node = node_map[name]
    node_id   = id
    node_ip   = node.ip
    node_port = node.port

    sc_loglevel = "DEBUG"
    sc_connmax = node.conn
    sc_service = "log,node"
    if name == "center" then
        sc_service = sc_service .. ",centers,cmdctl,cmds"
    else
        local center = node_map["center"]
        center_ip   = center.ip
        center_port = center.port
        sc_service = sc_service .. ",cmdctl"
    end
    local open = open_node_map[name]
    if open then
        gate_ip = open.ip
        gate_port = open.port
        gate_wbuffermax = open.wbuffer
        gate_clientmax  = open.clientmax
        gate_clientlive = open.clientlive
        gate_publish = 1
        gate_load  = open.load
        gate_need_verify = open.verify
        gate_handler = open.handler

        sc_connmax = sc_connmax + gate_clientmax
        --sc_service = sc_service .. ",gate"
    end
end
