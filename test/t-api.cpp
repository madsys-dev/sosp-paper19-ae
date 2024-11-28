#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <future>

#include "cxlmalloc-types.h"
#include "cxlmalloc.h"
#include "cxlmalloc-internal.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <vector>
#include "test_helper.h"

#include "cxlmalloc.h"
#include "cxlmalloc-internal.h"
#include "cxlmalloc-types.h"

using namespace std;
// 将macro 改成 global variable 以方便调用
// # define DATA_SIZE_BLOCK 128
// # define DATA_SIZE_MESSAGE 600

int counter;
int DATA_SIZE_BLOCK;
int DATA_SIZE_MESSAGE;

//# define THREAD2 1
//# define THREAD3 1

size_t length;
int shm_id;


void consumer(uint64_t queue_offset, std::promise<uint64_t> &offset)
{
    sleep(3);
    cxl_shm shm = cxl_shm(length, shm_id);
    shm.thread_init();
    void* start = shm.get_start();
    CXLRef r1 = shm.cxl_unwrap(queue_offset);
    offset.set_value(r1.get_tbr()->pptr);
}


void consumer_wrc(uint64_t queue_offset, std::promise<uint64_t> &offset, std::promise<std::chrono::time_point<std::chrono::system_clock> > &t_receiver)
{
    // sleep(3);
    
    cxl_shm shm = cxl_shm(length, shm_id);

    shm.thread_init();
    void* start = shm.get_start();


    auto t_start = std::chrono::high_resolution_clock::now();
    cxl_message_queue_t* q = (cxl_message_queue_t*) get_data_at_addr(start, queue_offset);
    // std::cout << "inside consumer_wrc: get_data_at_addr" << get_duration(std::chrono::high_resolution_clock::now(), t_start) << std::endl;
    POTENTIAL_FAULT
    // update receiver queue
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, shm.get_tls_offset());

    if(q->receiver_id == 0)
    {
        POTENTIAL_FAULT
        q->receiver_next = tls->receiver_queue;
        POTENTIAL_FAULT
        tls->receiver_queue = queue_offset;
        POTENTIAL_FAULT
        q->receiver_id = shm.get_thread_id();
    }
    std::vector<RootRef*> vec;
    for (int i = 0; i < counter; i ++) {
        RootRef* tbr = shm.thread_base_ref_alloc(tls);
        vec.push_back(tbr);
    }
    
    RootRef* tbr;
    // why this for loop?
    for (int i = 0; i < counter; i+=1) {
        tbr = vec[i];
        // auto t_start = std::chrono::high_resolution_clock::now();
        CXLRef r1 = shm.cxl_unwrap_wrc(queue_offset, q, tls, tbr);
        // std::cout << "duration of cxl_unwrap_wrc" << get_duration(std::chrono::high_resolution_clock::now(), t_start) << std::endl;
        
        uint64_t obj_offset = r1.data;
        // t_start = std::chrono::high_resolution_clock::now();
        CXLObj* cxl_obj1 = (CXLObj*)get_data_at_addr(start, obj_offset);
        // std::cout << "duration of get_data_at_addr: " << get_duration(std::chrono::high_resolution_clock::now(), t_start) << std::endl;
        
        // t_start = std::chrono::high_resolution_clock::now();
        while (cxl_obj1->writer_count != 0) {
        }
        // std::cout << "duration of while loop: " << get_duration(std::chrono::high_resolution_clock::now(), t_start) << std::endl;
        // offset.set_value(r1.get_tbr()->pptr);
//        t_receiver.set_value(t_receiver_temp);
    }
    // auto t_receiver_temp = static_cast<uint64_t>(time(NULL));
    auto t_receiver_temp = std::chrono::high_resolution_clock::now();
    t_receiver.set_value(t_receiver_temp);
}


// std::vector<size_t> test_sizes = {
//         8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536,
//         131072, 262144, 524288, 1048576, 2097152, 4194304, 8388608, 16777216,
//         33554432, 67108864, 134217728};


auto test_warpper() {
    length = (ZU(1) << 28);
    shm_id = shmget(100, length, IPC_CREAT|0664);
    shmctl(shm_id, IPC_RMID, NULL);

    // 命令行 第二个参数是：DATA_SIZE_BLOCK， 第三个参数是：DATA_SIZE_MESSAGE
    using namespace std;
    length = (ZU(1) << 28);
    shm_id = shmget(100, length, IPC_CREAT|0664);
    cxl_shm shm = cxl_shm(length, shm_id);
    shm.thread_init();

    CHECK_BODY("thread init") {
        shm.thread_init();
        result = (shm.get_thread_id() != 0);
    }
    
    // CHECK_BODY("malloc and free") {
    //     CXLRef ref = shm.cxl_malloc(32, 0);
    //     result = (ref.get_tbr() != NULL && ref.get_addr() != NULL);
    //     shm.cxl_free(true, (cxl_block*)(&ref));
    // };
    // CHECK_BODY("wrap ref") {
    //     CXLRef ref = shm.cxl_malloc(32, 0);
    //     uint64_t addr = shm.cxl_wrap(ref);
    //     result = (addr != 0);
    // };

    // CHECK_BODY("data transfer") {
    //     std::cout << "data transfer 1" << std::endl;
    //     CXLRef r1 = shm.cxl_malloc(1008, 0);
    //     std::cout << "data transfer 2" << std::endl;
    //     uint64_t queue_offset = shm.create_msg_queue(2);
    //     std::cout << "data transfer 3" << std::endl;
    //     shm.sent_to(queue_offset, r1);
        
    //     std::cout << "data transfer 4" << std::endl;
    //     std::promise<uint64_t> offset_2;
    //     std::promise<uint64_t> t_receiver;
        
    //     std::cout << "data transfer 5" << std::endl;
    //     std::thread t1(consumer_wrc, queue_offset, std::ref(offset_2), std::ref(t_receiver));
        
    //     std::cout << "data transfer 6" << std::endl;
    //     t1.join();
        
    //     std::cout << "data transfer 7" << std::endl;
    //     auto status = offset_2.get_future().get();
        
    //     std::cout << "data transfer 8" << "status"<<status<<"r1.get_tbr()" <<r1.get_tbr() << std::endl;
    //     result = (status == r1.get_tbr()->pptr);
    // };

    long t_duration;
    CHECK_BODY("t1 to t2") {
        void* start = shm.get_start();
        //std::cout << "t1 to t2 1111" << std::endl;
        uint64_t queue_offset1 = shm.create_msg_queue(2);
        // 起t1，循环等待queue的对象
        std::promise<uint64_t> offset_1;
        std::promise<std::chrono::time_point<std::chrono::system_clock>> t_receiver1;
        std::thread t1(consumer_wrc, queue_offset1, std::ref(offset_1), std::ref(t_receiver1));
        

        #ifdef THREAD2
        uint64_t queue_offset2 = shm.create_msg_queue(3);
        std::promise<uint64_t> offset_2;
        std::promise<std::chrono::time_point<std::chrono::system_clock>> t_receiver2;
        std::thread t2(consumer_wrc, queue_offset2, std::ref(offset_2), std::ref(t_receiver2));
        #endif

        #ifdef THREAD3
        uint64_t queue_offset3 = shm.create_msg_queue(4);
        std::promise<uint64_t> offset_3;
        std::promise<std::chrono::time_point<std::chrono::system_clock>> t_receiver3;
        std::thread t3(consumer_wrc, queue_offset3, std::ref(offset_3), std::ref(t_receiver3));
        #endif        

        // auto t_send = static_cast<uint64_t>(time(NULL));
        std::vector<CXLRef> block_vec;
        for (int i = 0; i < counter; i++) {
            CXLRef r = shm.cxl_malloc_wrc(DATA_SIZE_BLOCK, 0);
            block_vec.push_back(r);
        }


        sleep(5);
        auto t_start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < counter; i++) {
            CXLRef r1 = block_vec[i];
        //  std::cout << "t1 to t2 i:" << i << std::endl;
            //todo ： 发送的时间-最后一个receiver收到的时间
            
            
            //std::cout << "t1 to t2 111" << std::endl;
            //  << "t1 to t2 11 , r1.get_tbr()->pptr" << r1.get_tbr()->pptr << std::endl;
            
            //std::cout << "t1 to t2 2" << std::endl;
            uint64_t obj_offset = r1.data;
            CXLObj* cxl_obj = (CXLObj*)get_data_at_addr(start, obj_offset);
            
            // uint64_t obj_offset = r1.data;
            // CXLObj* cxl_obj = (CXLObj*)get_data_at_addr(start, obj_offset);
            //std::cout << "t1 to t2 3" << std::endl;
            while (cxl_obj->reader_count != 0 && cxl_obj->writer_count != 0) {
                
            }
            // std::cout << "before change , r1.get_tbr()->pptr" << r1.get_tbr()->pptr << std::endl;
            if (cxl_obj->reader_count == 0 && cxl_obj->writer_count == 0) {
                cxl_obj->writer_count++;
                // std::cout << "change 1: , r1.get_tbr()->pptr:" << r1.get_tbr()->pptr << std::endl;
                //r1 = shm.cxl_malloc_wrc(100, 0);
                //std::cout << "change 2: , r1.get_tbr()->pptr:" << r1.get_tbr()->pptr << std::endl;
                RootRef* tbr1 = (RootRef*) get_data_at_addr(start, r1.tbr);
                
                // std::cout << "change 3: , r1.get_tbr()->pptr:" << r1.get_tbr()->pptr << std::endl;
                tbr1->ref_cnt++;
    
                //r1.str_content = "bbb";
                cxl_obj->str_content = "bbb";
                    
                cxl_obj->writer_count--;
            }
            // std::cout << "after change , r1.get_tbr()->pptr" << r1.get_tbr()->pptr << std::endl;
            // std::cout << "t1 to t2 4.0,"<< t_send << "r1.get_tbr()->pptr" << r1.get_tbr()->pptr << std::endl;
            bool send_res1    = shm.sent_to(queue_offset1, r1);
            #ifdef THREAD2
            bool send_res2    = shm.sent_to(queue_offset2, r1);
            #endif
            #ifdef THREAD3
            bool send_res3    = shm.sent_to(queue_offset3, r1);
            #endif
            // std::cout << "t1 to t2 4.01 sendRes:" << (send_res1 ? "true" : "false") << std::endl;
            // std::cout << "t1 to t2 4.01 sendRes:" << (send_res2 ? "true" : "false") << std::endl;
        }
        //std::cout << "t1 to t2 4.01 sendRes:" << (send_res3 ? "true" : "false") << std::endl;
        //std::cout << "t1 to t2 4.02" << std::endl;
        t1.join();
        auto t_real_receive1 = t_receiver1.get_future().get();
        auto t_end = t_real_receive1;

        #ifdef THREAD2
        t2.join();
        auto t_real_receive2 = t_receiver2.get_future().get();
        t_end = t_end > t_real_receive2 ? t_end : t_real_receive2;
        #endif

        #ifdef THREAD3
        t3.join();
        auto t_real_receive3 = t_receiver3.get_future().get();
        t_end = t_end > t_real_receive3 ? t_end : t_real_receive3;
        #endif
        
        // auto t_receiver_final = static_cast<uint64_t>(time(NULL));
        //std::cout << "t1 to t2 4.1" << std::endl;
        // auto t_receive_2 = time(NULL);
        // auto status1 = offset_1.get_future().get();
        // auto status2 = offset_2.get_future().get();
        // auto status3 = offset_3.get_future().get();
        //std::cout << "t1 to t2 4.2" << std::endl;

        //std::cout << "t1 to t2 4.3" << std::endl;
    
        // std::cout << t_receiver_final << std::endl;
        // std::cout << t_send << std::endl;
        // auto t_all = t_receiver_final - t_send;
        // std::cout << "t_all :" << t_all  <<  std::endl; //",t_real_receive" << t_real_receive1 << ",t_send" << t_send  << ",t_receive_2" << t_receive_2 << "，status" << status1 <<"，r1.get_tbr()" << r1.get_tbr() << std::endl;
        // std::cout << "t_all_2 :" << t_all_2 << std::endl;

        
        // auto t_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();
        t_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();

        
        //std::cout << "t1 to t2 5" << std::endl;
        // CXLRef* r1_t2_1 = (CXLRef*)get_data_at_addr(start, status1);
        // CXLRef* r1_t2_2 = (CXLRef*)get_data_at_addr(start, status2);
        // CXLRef* r1_t2_3 = (CXLRef*)get_data_at_addr(start, status3);

        // uint64_t obj_offset_t2_1 = r1_t2_1->data;
        // uint64_t obj_offset_t2_2 = r1_t2_2->data;
        // uint64_t obj_offset_t2_3 = r1_t2_3->data;
        // CXLObj* cxl_obj_t2 = (CXLObj*)get_data_at_addr(start, obj_offset);
        // std::cout << "cxl_obj_t2_content :" << cxl_obj_t2->str_content << std::endl; 
        // cxl_obj_t2->reader_count--;
        // result = (status2 == r1.get_tbr()->pptr);
        //std::cout << "t1 to t2 6" << std::endl;
        shmctl(shm_id, IPC_RMID, NULL);
    };
    return t_duration;
}

int main(int argc, char *argv[])
{
    long result = 0;
    int iter = atoi(argv[1]);
    DATA_SIZE_BLOCK = atoi(argv[2]);
    DATA_SIZE_MESSAGE = atoi(argv[3]);
    counter = DATA_SIZE_MESSAGE / DATA_SIZE_BLOCK;
    for (int i = 0; i < iter; i++) {
        // 这里可能有数据溢出的风险。
        result += test_warpper();
    }

    /* std::cout << "+++++++++++++++++++++++++++++" << std::endl;
    std::cout << "DATA_SIZE_BLOCK: " << DATA_SIZE_BLOCK << std::endl;
    std::cout << "DATA_SIZE_MESSAGE: " << DATA_SIZE_MESSAGE << std::endl;
    std::cout << "average t_duration: " << result / (1.0 * iter) << std::endl;
    std::cout << "+++++++++++++++++++++++++++++" << std::endl;
    std::cout << std::endl; */
    
    std::cout << "Total: " <<result / (1.0 * iter) << std::endl;

    return print_test_summary();
}