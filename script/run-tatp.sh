for i in 1 2 4 8 16 24 32 48 64
do
	sleep 1
	./cxlmalloc-benchmark-tatp $i  >> tatp.txt
done

grep "MOPS" tatp.txt