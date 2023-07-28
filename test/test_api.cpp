#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <thread>
#include <unistd.h>
#include <future>

#include "cxlmalloc.h"
#include "cxlmalloc-internal.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include "test_helper.h"

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

int main()
{
    using namespace std;
    length = (ZU(1) << 28);
    shm_id = shmget(100, length, IPC_CREAT|0664);
    cxl_shm shm = cxl_shm(length, shm_id);
    shm.thread_init();

    CHECK_BODY("thread init") {
        shm.thread_init();
        result = (shm.get_thread_id() != 0);
    }

    CHECK_BODY("malloc and free") {
        CXLRef ref = shm.cxl_malloc(32, 0);
        result = (ref.get_tbr() != NULL && ref.get_addr() != NULL);
    };

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

    shmctl(shm_id, IPC_RMID, NULL);

    return print_test_summary();
}