#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <thread>
#include <unistd.h>
#include <future>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <iostream>

#include "cxlmalloc.h"
#include "cxlmalloc-internal.h"
#include "test_helper.h"

#include "fred.h"
#include "timer.h"
#include "AllocatorMacro.hpp"

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

void consumer_wrc(uint64_t queue_offset, std::promise<uint64_t> &offset, std::promise<uint64_t> &t_receiver)
{
    sleep(3);
    cxl_shm shm = cxl_shm(length, shm_id);
    shm.thread_init();
    void* start = shm.get_start();
    CXLRef r1 = shm.cxl_unwrap_wrc(queue_offset);
    auto t_receiver_temp = static_cast<uint64_t>(time(NULL));
    uint64_t obj_offset = r1.data;
    CXLObj* cxl_obj1 = (CXLObj*)((uintptr_t)start + obj_offset);
    while (cxl_obj1->writer_count != 0) {
        cxl_obj1->reader_count++;
    }
    offset.set_value(r1.get_tbr()->pptr);
    t_receiver.set_value(t_receiver_temp);
}

int main()
{
    
    using namespace std;
    length = (ZU(1) << 28);
    pm_init(length, shm_id);
    //shm_id = shmget(100, length, IPC_CREAT|0664);
    //shm_id = new shmget(100, length);
    cxl_shm* shm = new cxl_shm(length, shm_id);
    shm->thread_init();

    CHECK_BODY("thread init") {
        shm->thread_init();
        result = (shm->get_thread_id() != 0);
    }
    CXLRef ref = shm->cxl_malloc(1024, 0);

    //CHECK_BODY("malloc and free") {
        //CXLRef ref = shm.cxl_malloc(32, 0);
       // result = (ref.get_tbr() != NULL && ref.get_addr() != NULL);
    //};

    // CHECK_BODY("wrap ref") {
    //     CXLRef ref = shm.cxl_malloc(32, 0);
    //     uint64_t addr = shm.cxl_wrap(ref);
    //     result = (addr != 0);
    // };

    // CHECK_BODY("data transfer") {
    //     CXLRef r1 = shm.cxl_malloc(1008, 0);
    //     uint64_t queue_offset = shm.create_msg_queue(2);
    //     shm.sent_to(queue_offset, r1);
    //     std::promise<uint64_t> offset_2;
    //     std::thread t1(consumer, queue_offset, std::ref(offset_2));
    //     t1.join();
    //     auto status = offset_2.get_future().get();
    //     result = (status == r1.get_tbr()->pptr);
    // };

    // CHECK_BODY("t1 to t2") {
    //     std::cout << "t1 to t2 1" << std::endl;
    //     //todo ： 发送的时间-最后一个receiver收到的时间
    //     CXLRef r1 = shm.cxl_malloc_wrc(1, 0);
        
    //     std::cout << "t1 to t2 11" << std::endl;
    //     void* start = shm.get_start();
        
    //     std::cout << "t1 to t2 111" << std::endl;
    //     r1.str_content = "aaa";
        
    //     std::cout << "t1 to t2 1111" << std::endl;
    //     uint64_t queue_offset = shm.create_msg_queue(2);
        
    //     std::cout << "t1 to t2 11111" << std::endl;
        
        
        
    //     std::cout << "t1 to t2 2" << std::endl;
    //     uint64_t obj_offset = r1.data;
    //     CXLObj* cxl_obj = (CXLObj*)((uintptr_t)start + obj_offset);
    //     // 起t1，循环等待queue的对象
    //     std::promise<uint64_t> offset_2;
    //     std::promise<uint64_t> t_receiver;
    //     std::thread t1(consumer_wrc, queue_offset, std::ref(offset_2), std::ref(t_receiver));
        
        
    //     std::cout << "t1 to t2 3" << std::endl;
    //     while (cxl_obj->reader_count != 0 && cxl_obj->writer_count != 0) {
            
    //     }
    //     if (cxl_obj->reader_count == 0 && cxl_obj->writer_count == 0) {
    //         cxl_obj->writer_count++;
    //         r1 = shm.cxl_malloc_wrc(100, 0);
    //         RootRef* tbr1 = (RootRef*) get_data_at_addr(start, r1.tbr);
    //         tbr1->ref_cnt++;
    //         r1.str_content = "bbb";
    //         cxl_obj->writer_count--;
    //     }
            
    //     std::cout << "t1 to t2 4" << std::endl;
    //     auto t_send = static_cast<uint64_t>(time(NULL));
    //     shm.sent_to(queue_offset, r1);
    //     t1.join();
    //     auto t_receive_2 = time(NULL);
    //     auto status = offset_2.get_future().get();
    //     auto t_real_receive = t_receiver.get_future().get();
    //     auto t_all = t_real_receive - t_send;
    //     auto t_all_2 = t_receive_2 - t_send;
    

    
    //     std::cout << "t_all " << t_all << std::endl;
    //     std::cout << "t_all_2" << t_all_2 << std::endl;

    //     std::cout << "t1 to t2 5" << std::endl;
    //     CXLRef r1_t2 = shm.get_ref(status);
    //     uint64_t obj_offset_t2 = r1_t2.data;
    //     CXLObj* cxl_obj_t2 = (CXLObj*)((uintptr_t)start + obj_offset);
    //     cxl_obj_t2->reader_count--;
    //     result = (status == r1.get_tbr()->pptr);
    //     std::cout << "t1 to t2 6" << std::endl;
        
    // }
    shmctl(shm_id, IPC_RMID, NULL);

    return print_test_summary();
}