require "config_base"
def_node("gateload", 2)
sh_module=sh_module..",loadbalance:gate_load"

loadbalance_target="gate"
loadbalance_subshriber="route"
