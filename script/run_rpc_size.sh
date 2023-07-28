for i in 64 128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576 2097152
do
    ./cxlmalloc-benchmark-rpc_client 1 $i >> rpc_client_size.out &
    sleep 20
    numactl --cpubind=1 ./cxlmalloc-benchmark-rpc_server 1 $i >> rpc_server_size.out 
    sleep 5
done

grep "throughput" rpc_server_size.out 