require "config_base"
def_node("auth", 30)
sh_module=sh_module..",auth,redisproxy:rpacc"

rpacc_requester="auth"
rpacc_sharding_mod=1
rpacc_redis_auth="shaco@1986#0621"
rpacc_redis_list="127.0.0.1:6380"
