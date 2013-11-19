sc_loglevel="DEBUG" 
sc_connmax=1100 
sc_service="log,dispatcher,node,centers,gate,cmds,cmdctl" 

node_type="center" 
node_sid=0 
node_ip="127.0.0.1" 
node_port=8000 

gate_ip="192.168.1.140" 
gate_port=18000 
gate_clientmax=100 
gate_clientlive=3600 
gate_noneed_verify=0
gate_handler="cmds" 
