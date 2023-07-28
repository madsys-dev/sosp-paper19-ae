for i in 1 2 4 8 16 24 32 48 64
do
	sleep 1
	./cxlmalloc-benchmark-smallbank $i  >> smallbank.txt
done

grep "MOPS" smallbank.txt