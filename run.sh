#!/bin/bash
RESULT_FILE="result_t1.log"
rm "$RESULT_FILE"
# echo "test single thread consumer without thread" >> result.log

# DATA_SIZE_MESSAGE=("16" "32" "64" "128" "160" "192" "224" "256" "288" "320" "512" "1024" "2048" "4096")
DATA_SIZE_MESSAGE=("1024" "4096" "16384" "32768")

# for size in "${DATA_SIZE_MESSAGE[@]}"
# do
#     echo -n "DATA_SIZE_MESSAGE" $size  " : ">> result.log
#     # 参数说明：第一个参数是运行次数，最大值为1024， 第二个参数是：DATA_SIZE_BLOCK, 第三个参数是：DATA_SIZE_MESSAGE
#     ./build/cxlmalloc-test-api 3 128 $size >> result.log
# done

# message size 可以小于 block size吗？
DATA_SIZE_BLOCK=("64" "128" "256" "512" "1024")
for size in "${DATA_SIZE_MESSAGE[@]}"
do
    for size_block in "${DATA_SIZE_BLOCK[@]}"
    do
        printf "DATA_SIZE_BLOCK: $size_block DATA_SIZE_MESSAGE $size " >> "$RESULT_FILE"
        ./build/cxlmalloc-test-api 3 $size_block $size >> "$RESULT_FILE"
    done
done