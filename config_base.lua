-- shaco config base
-- only string value will be read
-- the key prefix by "_" will no be read

local iip = "127.0.0.1"
local oip = "192.168.1.145"
log_dir = "/home/game/log"

node_map = {
center   = {ip=iip, port=8000, conn=1000},
login    = {ip=iip, port=8100, conn=1000, sub="rpacc"},
gateload = {ip=iip, port=8200, conn=1000, sub="login"},
gate     = {ip=iip, port=8300, conn=1000, sub="gateload,world"},
world    = {ip=iip, port=8400, conn=1000, sub="rpuser"},
game     = {ip=iip, port=8500, conn=1000, sub="world"},
bmdb     = {ip=iip, port=8600, conn=1000, sub="rpacc,rpuser,rprank"},
rpacc    = {ip=iip, port=8700, conn=1000},
rpuser   = {ip=iip, port=8800, conn=1000},
rprank   = {ip=iip, port=8900, conn=1000},
}

open_node_map = {
center = {ip=oip, port=18000, handler="cmds",  clientmax=100,   clientlive=60, wbuffer=0, verify=0},
login  = {ip=oip, port=18100, handler="login", clientmax=10000, clientlive=0,  wbuffer=0},
gate   = {ip=oip, port=18300, handler="gate",  clientmax=10000, clientlive=6,  wbuffer=128*1024, load=1},
game   = {ip=oip, port=18500, handler="game",  clientmax=5000,  clientlive=6,  wbuffer=128*1024}, 
}

function def_node(name, sid)
    local node = node_map[name]
    node_type = name
    node_sid  = sid
    node_ip   = node.ip
    node_port = node.port
    node_sub  = node.sub

    sc_loglevel = "INFO"
    sc_connmax = node.conn
    sc_service = "log,dispatcher,node"
    if name == "center" then
        sc_service = sc_service .. ",centers,cmdctl,cmds"
    else
        local center = node_map["center"]
        center_ip   = center.ip
        center_port = center.port
        sc_service = sc_service .. ",centerc,cmdctl"
    end
    local open = open_node_map[name]
    if open then
        gate_ip = open.ip
        gate_port = open.port
        gate_wbuffermax = open.wbuffer
        gate_clientmax  = open.clientmax
        gate_clientlive = open.clientlive
        gate_need_load  = open.load
        gate_need_verify = open.verify
        gate_handler = open.handler

        sc_connmax = sc_connmax + gate_clientmax
        sc_service = sc_service .. ",gate," .. gate_handler
    end
end
