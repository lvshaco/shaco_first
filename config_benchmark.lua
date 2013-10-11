shaco = {
host_loglevel="DEBUG",
host_connmax=11000,
host_service="benchmark",

echo_ip="127.0.0.1",
--echo_ip="192.168.1.140",
echo_port=10001,

benchmark_client_max=1, -- active client
benchmark_query=1,     -- query times
benchmark_packet_size=4, -- packet size in bytes
}
