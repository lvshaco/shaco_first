require "config_base"
def_node("gate2", 52)
sh_module=sh_module..",gate"

-- (>= gate_load_size) or (>= gate_load_max_interval), then upate load
gate_load_size=100
gate_load_max_interval=1000 -- actural [1000, 2000)
