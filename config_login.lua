sc_loglevel="INFO" 
sc_connmax=11000 
sc_service="log,dispatcher,node,centerc,cmdctl,gate,login"

log_dir="/home/game/log"

node_type="login" 
node_sid=0 
node_ip="127.0.0.1" 
node_port=8600 
node_sub="redisproxy" 
center_ip="127.0.0.1" 
center_port=8000 
gate_ip="192.168.1.145" 
gate_port=18600 
gate_clientmax=10000 
gate_clientlive=0  -- no live time triggered
gate_handler="login" 
