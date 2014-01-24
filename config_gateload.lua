require "config_base"
def_node("gateload", 2)
sc_service=sc_service..",loadbalance:gate_load"

loadbalance_target="gate"
loadbalance_subscriber="route"
