require "config_base"
def_node("gateload", 2)
sh_module=sh_module..",loadbalance:gate_load"

loadbalance_publisher="gate"
loadbalance_subscriber="route"
