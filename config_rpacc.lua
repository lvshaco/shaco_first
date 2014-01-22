require "config_base"
def_node("rpacc", 200)

sc_service=sc_service..",redisproxy:rpacc"
redis_ip="127.0.0.1" 
redis_port=6379 
redis_auth="shaco@1986#0621"
redis_requester="auth"
