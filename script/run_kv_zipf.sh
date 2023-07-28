echo "Zipf=.9"
for i in 1 2 4 8
do
    ./cxlmalloc-benchmark-kv_zipf $i 3 0.9 >> zipf9.txt
done

grep "Throughput" zipf9.txt

echo "Zipf=.99"
for i in 1 2 4 8
do
    ./cxlmalloc-benchmark-kv_zipf $i 3 0.99 >> zipf99.txt
done

grep "Throughput" zipf99.txt

echo "Zipf=.5"
for i in 1 2 4 8
do
    ./cxlmalloc-benchmark-kv_zipf $i 3 0.5 >> zipf50.txt
done

grep "Throughput" zipf50.txt
