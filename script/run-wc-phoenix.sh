for i in 2 4 8 16 24 32 48 64
do
        export MR_NUMTHREADS=$i
        export MR_NUMPROCS=$i
        sleep 1
        time ./wordcount 1GB.txt >> res-ph.txt
done

grep "real" res-ph.txt
