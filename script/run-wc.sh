for i in 1 2 4 8 16 24 32 48 64
do
	sleep 1
	./cxlmalloc-benchmark-wc $i $i 1024000000 1>>wc.txt 2>>err.txt
done

grep "TIME" wc.txt