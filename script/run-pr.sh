path=/home/sima.mt/soc-LiveJournal1.txt

for i in 2 4 8 16 24 32 48 64
do
	sleep 1
	# ./cxlmalloc-benchmark-pr $i $i 875713 5105039 /root/web.txt >> out-pr.txt
	./cxlmalloc-benchmark-pr $i $i 4847571 68993773 $path 1>>pr.txt 2>>err.txt

done

# grep "TIME" out-pr.txt
grep "TIME" pr.txt
