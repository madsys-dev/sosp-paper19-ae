for i in 1 2 4 8 16 24 32 48 64
do
    ./cxlmalloc-benchmark-kv $i 0 >> kv.txt
done

grep "Throughput" kv.txt