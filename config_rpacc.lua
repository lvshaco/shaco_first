require "config_base"
def_node("rpacc", 200)

sh_module=sh_module..",redisproxy:rpacc"

redis_auth="shaco@1986#0621"
redis_requester="auth"

redis_list="127.0.0.1:6379"
