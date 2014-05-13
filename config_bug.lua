require "config_base"
def_node("bug", 6)
sh_module=sh_module..",bug,redisproxy:rpbug"
bug_source="watchdog"

rpbug_requester="bug"
rpbug_redis_auth="shaco@1986#0621"
rpbug_redis_list="127.0.0.1:6391"
