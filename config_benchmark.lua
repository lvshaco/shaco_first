shaco = {
host_loglevel="DEBUG",
host_connmax=11000,
host_service="benchmark",

--echo_ip="127.0.0.1",
echo_ip="192.168.1.140",
--echo_port=10001,
echo_port=9999,
benchmark_client_max=10000, -- active client
benchmark_query=100000,     -- query times
--benchmark_query_first=10000,
benchmark_packet_size=256, -- packet size in bytes
}
