sc_loglevel="DEBUG"
sc_connmax=5100
sc_service="log,dispatcher,node,centerc,cmdctl,cmdctlgame,tpltgame,gate,game"

cmdctl_handler="cmdctlgame"
tplt_handler="tpltgame"

node_type="game"
node_sid=0
node_ip="127.0.0.1"
node_port=8200
node_sub="world"
center_ip="127.0.0.1"
center_port=8000
--gate_ip="127.0.0.1"
gate_ip="192.168.1.140"
gate_port=18200
gate_wbuffermax=128*1024
gate_clientmax=5000
gate_clientlive=6000
gate_handler="game"
