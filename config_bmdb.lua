sc_service=sc_service.."benchmarkdb"

benchmark_query=100000
benchmark_query_init=1000

require "config_base"
def_node("bmdb", 0)
--./shaco config_benchmarkdb.lua --benchmark_query_init 0
--./shaco-cli --cmd "all all db acca 1 1000000 1000"
