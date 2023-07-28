echo "begin test" >  recovery_check.out
for i in {1..8}
do
    echo "begin test" >  recovery_test_$i.out
done

for k in {1..10}
do
    for i in {1..10000}
    do
        echo "$i" >> recovery_check.out
        ./shm_init
        for((j=1; j<=8; j++))
        do
        {
            ./recovery_test $j >> recovery_test_$j.out
        } & done
        wait
        ./recovery_check >> recovery_check.out
        if [ $? -eq 139 ]; then
            echo "segmentation fault"
            exit 1
        fi

    done
done
