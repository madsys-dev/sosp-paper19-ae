for i in 1 2 4 8 16 24 32 48 64
do
	sleep 1
	./cxlmalloc-benchmark-km $i $i 500000 8 1024 1>>km.txt 2>>err.txt
done

grep "TIME" km.txt