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

// 将macro 改成 global variable 以方便调用
// # define DATA_SIZE_BLOCK 128
// # define DATA_SIZE_MESSAGE 600

int counter;
int DATA_SIZE_BLOCK;
int DATA_SIZE_MESSAGE;

std::atomic<bool> firstSendDone(false);

// std::atomic_flag mutex_flag = ATOMIC_FLAG_INIT;

std::atomic<bool> firstUnwrapDone(false);

std::mutex mutex1;
// # define THREAD2 1
// # define THREAD3 1
// # define THREAD4 1
// # define THREAD5 1
// # define THREAD6 1

size_t length;
int shm_id;

void consumer_wrc(uint64_t queue_offset, std::promise<uint64_t> &offset, std::promise<std::chrono::time_point<std::chrono::system_clock> > &t_receiver)
{
    // sleep(3);
    
    cxl_shm shm = cxl_shm(length, shm_id);

    shm.thread_init();
    void* start = shm.get_start();
    
    // auto t_start = std::chrono::high_resolution_clock::now();
    // cxl_message_queue_t* q = (cxl_message_queue_t*) get_data_at_addr(start, queue_offset);
    // // std::cout << "inside consumer_wrc: get_data_at_addr" << get_duration(std::chrono::high_resolution_clock::now(), t_start) << std::endl;
    // POTENTIAL_FAULT
    // // update receiver queue
    // cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, shm.get_tls_offset());
    // //std::cout << "consumer_wrc length :" << length  << " ,shm_id: "<< shm_id <<" ,queue_offset: " << queue_offset  << " ,start: " << start << " , q:" << q << " ,tls: " << tls << " ,q->receiver_id: " << q->receiver_id << " ,shm.get_thread_id(): " << shm.get_thread_id() << std::endl;
    
    // if(q->receiver_id == 0)
    // {
    //     POTENTIAL_FAULT
    //     q->receiver_next = tls->receiver_queue;
    //     POTENTIAL_FAULT
    //     tls->receiver_queue = queue_offset;
    //     POTENTIAL_FAULT
    //     q->receiver_id = shm.get_thread_id();
    // }
    // std::vector<RootRef*> vec;
    // for (int i = 0; i < counter; i ++) {
    //     RootRef* tbr0 = shm.thread_base_ref_alloc(tls);
    //     RootRef* tbr = shm.thread_base_ref_alloc();
    //     vec.push_back(tbr);
    // }
    
    // RootRef* tbr;

    // while (firstUnwrapDone.load(std::memory_order_release));
    // firstUnwrapDone.store(true, std::memory_order_release);
    
    // std::atomic_thread_fence(std::memory_order_release);
    for (int i = 0; i < counter; i++) {
        //while (mutex_flag.test_and_set(std::memory_order_acquire)) {}
        // std::lock_guard<std::mutex> guard(mutex1);
        // tbr = vec[i];
        //std::cout << "\n before: cxl_unwrap_mend i:" << i << std::endl;
        CXLRef r1 = shm.cxl_unwrap_mend(queue_offset);
        
        //std::cout << "\n after: cxl_unwrap_mend" << std::endl;
        uint64_t obj_offset = r1.data;
        CXLObj* cxl_obj1 = (CXLObj*)get_data_at_addr(start, obj_offset);
        
        //std::cout << "before: cxl_obj1->writer_count != 0" << std::endl;
        while (cxl_obj1->writer_count != 0) {
        }
        
        // mutex_flag.clear(std::memory_order_release);
        //std::cout << "after: cxl_obj1->writer_count != 0" << std::endl;
    }
    // firstUnwrapDone.store(false, std::memory_order_release);
    auto t_receiver_temp = std::chrono::high_resolution_clock::now();
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(t_receiver_temp.time_since_epoch()).count();
    std::cout << "t_receiver_temp time in nanoseconds: " << nanoseconds << std::endl;
    t_receiver.set_value(t_receiver_temp);
}

// auto send(cxl_shm shm, uint64_t queue_offset ,CXLRef r1) {
  
//     std::lock_guard<std::mutex> guard(mutex1);
//     while (firstSendDone.load(std::memory_order_acquire)){};
//     firstSendDone.store(true, std::memory_order_release);
//     bool send_res2    = shm.sent_to(queue_offset, r1);
//     firstSendDone.store(false, std::memory_order_release);
// }

auto test_warpper() {
    length = (ZU(1) << 28);
    shm_id = shmget(100, length, IPC_CREAT|0664);
    shmctl(shm_id, IPC_RMID, NULL);

    using namespace std;
    length = (ZU(1) << 28);
    shm_id = shmget(100, length, IPC_CREAT|0664);
    cxl_shm shm = cxl_shm(length, shm_id);
    shm.thread_init();

    CHECK_BODY("thread init") {
        shm.thread_init();
        result = (shm.get_thread_id() != 0);
    }

    long t_duration;
    CHECK_BODY("t1 to t2") {
        void* start = shm.get_start();
        uint64_t queue_offset1 = shm.create_msg_queue(2);
        // 起t1，循环等待queue的对象
        std::promise<uint64_t> offset_1;
        std::promise<std::chrono::time_point<std::chrono::system_clock>> t_receiver1;
        std::thread t1(consumer_wrc, queue_offset1, std::ref(offset_1), std::ref(t_receiver1));
        
        #ifdef THREAD2
        uint64_t queue_offset2 = shm.create_msg_queue(4);
        std::promise<uint64_t> offset_2;
        std::promise<std::chrono::time_point<std::chrono::system_clock>> t_receiver2;
        
        std::thread t2(consumer_wrc, queue_offset2, std::ref(offset_2), std::ref(t_receiver2));
        #endif

        #ifdef THREAD3
        uint64_t queue_offset3 = shm.create_msg_queue(6);
        std::promise<uint64_t> offset_3;
        std::promise<std::chrono::time_point<std::chrono::system_clock>> t_receiver3;
        std::thread t3(consumer_wrc, queue_offset3, std::ref(offset_3), std::ref(t_receiver3));
        #endif       

        #ifdef THREAD4
        uint64_t queue_offset4 = shm.create_msg_queue(8);
        std::promise<uint64_t> offset_4;
        std::promise<std::chrono::time_point<std::chrono::system_clock>> t_receiver4;
        std::thread t4(consumer_wrc, queue_offset4, std::ref(offset_4), std::ref(t_receiver4));
        #endif   

        #ifdef THREAD5
        uint64_t queue_offset5 = shm.create_msg_queue(10);
        std::promise<uint64_t> offset_5;
        std::promise<std::chrono::time_point<std::chrono::system_clock>> t_receiver5;
        std::thread t5(consumer_wrc, queue_offset5, std::ref(offset_5), std::ref(t_receiver5));
        #endif

        #ifdef THREAD6
        uint64_t queue_offset6 = shm.create_msg_queue(12);
        std::promise<uint64_t> offset_6;
        std::promise<std::chrono::time_point<std::chrono::system_clock>> t_receiver6;
        std::thread t6(consumer_wrc, queue_offset6, std::ref(offset_6), std::ref(t_receiver6));
        #endif   

        std::vector<CXLRef> block_vec;
        for (int i = 0; i < counter; i++) {
            CXLRef r = shm.cxl_malloc_wrc(DATA_SIZE_BLOCK, 0);
            block_vec.push_back(r);
        }

        sleep(1);
        auto t_start = std::chrono::high_resolution_clock::now();
        
        auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(t_start.time_since_epoch()).count();
        std::cout << "t_start time in nanoseconds: " << nanoseconds << std::endl;
        for (int i = 0; i < counter; i++) {
            CXLRef r1 = block_vec[i];
            uint64_t obj_offset = r1.data;
            CXLObj* cxl_obj = (CXLObj*)get_data_at_addr(start, obj_offset);

            while (cxl_obj->reader_count != 0 && cxl_obj->writer_count != 0) {
                
            }
            if (cxl_obj->reader_count == 0 && cxl_obj->writer_count == 0) {
                cxl_obj->writer_count++;
                RootRef* tbr1 = (RootRef*) get_data_at_addr(start, r1.tbr);
                tbr1->ref_cnt++;
                cxl_obj->str_content = "bbb";
                cxl_obj->writer_count--;
            }
            
            
            //std::cout << "\n sent_to start i:" << i << std::endl;
            // while (firstSendDone.load(std::memory_order_release));
            // firstSendDone.store(true, std::memory_order_release);
            // std::atomic_thread_fence(std::memory_order_acquire);
            bool send_res1    = shm.sent_to(queue_offset1, r1);
            // firstSendDone.store(false, std::memory_order_release);
            // send(shm, queue_offset1, r1);
            
            //std::cout << "sent_to after i:" << i  << std::endl;

            #ifdef THREAD2
            while (firstSendDone.load(std::memory_order_release));
            firstSendDone.store(true, std::memory_order_release);
            std::atomic_thread_fence(std::memory_order_acquire);
            bool send_res2    = shm.sent_to(queue_offset2, r1);
            firstSendDone.store(false, std::memory_order_release);

//            send(shm, queue_offset2, r1);
            #endif

            #ifdef THREAD3
            //send(shm, queue_offset3, r1);
            // sleep(1);
            while (firstSendDone.load(std::memory_order_acquire));
            firstSendDone.store(true, std::memory_order_release);
            std::atomic_thread_fence(std::memory_order_acquire);
            bool send_res3    = shm.sent_to(queue_offset3, r1);
            firstSendDone.store(false, std::memory_order_release);
            // sleep(1);
            #endif

            #ifdef THREAD4
            //send(shm, queue_offset3, r1);
            // sleep(1);
            while (firstSendDone.load(std::memory_order_acquire));
            firstSendDone.store(true, std::memory_order_release);
            std::atomic_thread_fence(std::memory_order_acquire);
            bool send_res4    = shm.sent_to(queue_offset4, r1);
            firstSendDone.store(false, std::memory_order_release);
            // sleep(1);
            #endif

             #ifdef THREAD5
            //send(shm, queue_offset3, r1);
            // sleep(1);
            while (firstSendDone.load(std::memory_order_acquire));
            firstSendDone.store(true, std::memory_order_release);
            std::atomic_thread_fence(std::memory_order_acquire);
            bool send_res5    = shm.sent_to(queue_offset5, r1);
            firstSendDone.store(false, std::memory_order_release);
            // sleep(1);
            #endif

             #ifdef THREAD6
            //send(shm, queue_offset3, r1);
            // sleep(1);
            while (firstSendDone.load(std::memory_order_acquire));
            firstSendDone.store(true, std::memory_order_release);
            std::atomic_thread_fence(std::memory_order_acquire);
            bool send_res6    = shm.sent_to(queue_offset6, r1);
            firstSendDone.store(false, std::memory_order_release);
            // sleep(1);
            #endif
        }
        t1.join();

        auto t_real_receive1 = t_receiver1.get_future().get();
        
        auto nanoseconds1 = std::chrono::duration_cast<std::chrono::nanoseconds>(t_real_receive1.time_since_epoch()).count();
        std::cout << "t_receiver_temp time in nanoseconds: " << nanoseconds1 << std::endl;
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

        #ifdef THREAD4
        t4.join();
        auto t_real_receive4 = t_receiver4.get_future().get();
        t_end = t_end > t_real_receive4 ? t_end : t_real_receive4;
        #endif

        #ifdef THREAD5
        t5.join();
        auto t_real_receive5 = t_receiver5.get_future().get();
        t_end = t_end > t_real_receive5 ? t_end : t_real_receive5;
        #endif

        
        #ifdef THREAD6
        t6.join();
        auto t_real_receive6 = t_receiver6.get_future().get();
        t_end = t_end > t_real_receive6 ? t_end : t_real_receive6;
        #endif

        t_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();
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
    if (counter == 0) {
        counter = 1;
    }
    for (int i = 0; i < iter; i++) {
        // 这里可能有数据溢出的风险
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