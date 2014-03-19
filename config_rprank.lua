require "config_base"
def_node("rprank", 202)

sh_module=sh_module..",redisproxy:rprank"

redis_auth="shaco@1986#0621"
redis_requester="hall"

redis_list="127.0.0.1:6379"
