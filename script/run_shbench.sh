for threads in 1 2 4 6 10 16 20 24 32 40 48 62 72 80 84 88
do
	sleep 1
	./cxlmalloc-benchmark-sh6bench 10000 64 400 $threads 2>>error.txt
done