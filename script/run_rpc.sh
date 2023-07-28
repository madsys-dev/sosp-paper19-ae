for i in 1 2 4 8 16 24 32 48
do
    ./cxlmalloc-benchmark-rpc_client $i >> rpc_client.out &
    sleep 20
    ./cxlmalloc-benchmark-rpc_server $i >> rpc_server.out 
    sleep 5
done

grep "throughput" rpc_server.out 