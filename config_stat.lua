require "config_base"
def_node("stat", 8)
sh_module=sh_module..",stat,redisproxy:rpstat"

rpstat_requester="hall"
rpstat_sharding_mod=5
rpstat_redis_auth="shaco@1986#0621"
rpstat_redis_list="127.0.0.1:6392:0,127.0.0.1:6392:1,127.0.0.1:6392:2,127.0.0.1:6392:3,127.0.0.1:6392:4"
