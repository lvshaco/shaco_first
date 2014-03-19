require "config_base"
def_node("hall", 80)
sh_module=sh_module..",hall,redisproxy:rpuser,redisproxy:rpuseruni,redisproxy:rprank"

rpuser_requester="hall,benchmarkdb"
rpuser_sharding_mod=32
rpuser_redis_auth="shaco@1986#0621"
rpuser_redis_list="127.0.0.1:6400,127.0.0.1:6401,127.0.0.1:6402,127.0.0.1:6403,127.0.0.1:6404,127.0.0.1:6405,127.0.0.1:6406,127.0.0.1:6407,127.0.0.1:6408,127.0.0.1:6409,127.0.0.1:6410,127.0.0.1:6411,127.0.0.1:6412,127.0.0.1:6413,127.0.0.1:6414,127.0.0.1:6415,127.0.0.1:6416,127.0.0.1:6417,127.0.0.1:6418,127.0.0.1:6419,127.0.0.1:6420,127.0.0.1:6421,127.0.0.1:6422,127.0.0.1:6423,127.0.0.1:6424,127.0.0.1:6425,127.0.0.1:6426,127.0.0.1:6427,127.0.0.1:6428,127.0.0.1:6429,127.0.0.1:6430,127.0.0.1:6431"

rpuseruni_requester="hall,benchmarkdb"
rpuseruni_sharding_mod=1
rpuseruni_redis_auth="shaco@1986#0621"
rpuseruni_redis_list="127.0.0.1:6390"

rprank_requester="hall,benchmarkdb"
rprank_sharding_mod=1
rprank_redis_auth="shaco@1986#0621"
rprank_redis_list="127.0.0.1:6391"
