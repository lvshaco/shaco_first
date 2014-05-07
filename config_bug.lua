require "config_base"
def_node("bug", 8)
sh_module=sh_module..",bug,gate:bug_gate,redisproxy:rpbug"

rpbug_requester="bug"
rpbug_redis_auth="shaco@1986#0621"
rpbug_redis_list="127.0.0.1:6391"
