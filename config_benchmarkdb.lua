sc_loglevel="INFO"
sc_connmax=100
sc_service="log,dispatcher,node,centerc,cmdctl,benchmarkdb"

node_type="benchmarkdb"
node_sid=0
node_ip="127.0.0.1"
node_port=8500
node_sub="redisproxy"

center_ip="127.0.0.1"
center_port=8000

benchmark_query=100000
benchmark_query_init=1000

--./shaco config_benchmark --benchmark_query_init 0
--./shaco-cli --cmd="all all db acca 1 1000000"
