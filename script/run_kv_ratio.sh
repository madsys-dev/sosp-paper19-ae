for i in 0 1 2 3 4 9
do
    ./cxlmalloc-benchmark-kv 8 $i >> kv_ratio.txt
done

grep "Throughput" kv_ratio.txt