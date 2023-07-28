for i in 1 2 4 8 16 24 32 48 64
do
    ./cxlmalloc-benchmark-kv_baseline $i 
done

grep "Throughput" kv.txt