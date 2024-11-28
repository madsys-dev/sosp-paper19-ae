#include <assert.h>
#include <errno.h>
#include <future>
#include <stdbool.h>
#include <stdint.h>
#include <thread>
#include <unistd.h>

#include "cxlmalloc-internal.h"
#include "cxlmalloc.h"
#include "test_helper.h"
#include <sys/ipc.h>
#include <sys/shm.h>

// // # define FLUSH(addr) asm volatile('clwb (0)""r"(addr))
// #define FLUSH(addr) asm volatile("clwb (%0)" : : "r"(addr));

// # define FENCE asm volatile("sfence":::"memory")

size_t length;
int shm_id;

int DATA_SIZE_BLOCK = 128;
int DATA_SIZE_MESSAGE = 256;
int counter = DATA_SIZE_MESSAGE / DATA_SIZE_BLOCK;

std::atomic<bool> firstSendDone(false);
std::atomic<bool> firstUnwrapDone(false);

std::mutex mutex1;
std::atomic_flag lock = ATOMIC_FLAG_INIT;

void consumer(uint64_t queue_offset, std::promise<uint64_t> &offset) {
  // sleep(1);
  std::cout << "consumer0" << std::endl;
  cxl_shm shm = cxl_shm(length, shm_id);
  std::cout << "consumer1" << std::endl;
  shm.thread_init();
  std::cout << "consumer2" << std::endl;
  void *start = shm.get_start();
  std::cout << "consumer3 start: " << start << std::endl;
  auto result = 1;
  
  while (firstUnwrapDone.load(std::memory_order_release));
  firstUnwrapDone.store(true, std::memory_order_release);
  for (int i = 0; i < counter; i++) {
    CXLRef r1 = shm.cxl_unwrap_mend(queue_offset);
  }
  firstUnwrapDone.store(false, std::memory_order_release);
  offset.set_value(result);
}

// void consumer_t2(uint64_t queue_offset, std::promise<uint64_t> &offset) {
//   // sleep(1);
//   std::cout << "consumer_t2 consumer0" << std::endl;
//   cxl_shm shm = cxl_shm(length, shm_id);
//   std::cout << "consumer_t2 consumer1" << std::endl;
//   shm.thread_init();
//   std::cout << "consumer_t2 consumer2" << std::endl;
//   void *start = shm.get_start();
//   std::cout << "consumer_t2 consumer3  start: " << start << std::endl;

//   auto result = 1;
//   for (int i = 0; i < counter; i++) {
//     while (!firstUnwrapDone.load(std::memory_order_release));
//     std::atomic_thread_fence(std::memory_order_acquire);

//     // FENCE;
//     CXLRef r1 = shm.cxl_unwrap_mend(queue_offset);
//     // std::atomic_thread_fence(std::memory_order_seq_cst);

//     // FLUSH(&r1);
//      std::atomic_thread_fence(std::memory_order_release);
//      firstUnwrapDone.store(false, std::memory_order_release);
//     std::cout << "consumer_t2 consumer4 r1.get_tbr():" << r1.get_tbr()
//               << std::endl;
//     std::cout << "consumer_t2 consumer4 r1.get_tbr()->pptr:"
//               << r1.get_tbr()->pptr << std::endl;
//     std::cout << "consumer_t2 consumer5" << std::endl;
//   }
//   offset.set_value(result);
// }

int main() {
  using namespace std;
  length = (ZU(1) << 28);
  shm_id = shmget(100, length, IPC_CREAT | 0664);

  shmctl(shm_id, IPC_RMID, NULL);

  length = (ZU(1) << 28);
  shm_id = shmget(100, length, IPC_CREAT | 0664);
  cxl_shm shm = cxl_shm(length, shm_id);
  std::atomic_thread_fence(std::memory_order_seq_cst);

  std::cout << "1" << std::endl;
  shm.thread_init();

  std::cout << "1" << std::endl;

  uint64_t queue_offset1 = shm.create_msg_queue(2);
  uint64_t queue_offset2 = shm.create_msg_queue(4);
  if (queue_offset1 == 0 || queue_offset2 == 0) {
    std::cerr << "Failed to create message queues" << std::endl;
    return 1;
  }
  std::promise<uint64_t> offset_1;
  std::promise<uint64_t> offset_2;
  std::thread t1(consumer, queue_offset1, std::ref(offset_1));
  // sleep(1);
  // std::atomic_thread_fence(std::memory_order_seq_cst);

  std::thread t2(consumer, queue_offset2, std::ref(offset_2));
  CXLRef r1 = shm.cxl_malloc(DATA_SIZE_BLOCK, 0);
  // std::atomic_thread_fence(std::memory_order_seq_cst);

  for (int i = 0; i < counter; i++) {

    std::cout << "send start t1: " << queue_offset1 << std::endl;

    while (firstSendDone.load(std::memory_order_release));
    std::atomic_thread_fence(std::memory_order_acquire);

    // FENCE;
    shm.sent_to(queue_offset1, r1);
    // sleep(1);
    // std::atomic_thread_fence(std::memory_order_seq_cst);

    // FLUSH(&r1);
    //  FENCE;
     std::atomic_thread_fence(std::memory_order_release);
     firstSendDone.store(true, std::memory_order_release);

    while (!firstSendDone.load(std::memory_order_release));
    std::atomic_thread_fence(std::memory_order_acquire);

    std::cout << "send start t2: " << queue_offset1 << std::endl;
    shm.sent_to(queue_offset2, r1);
    // std::atomic_thread_fence(std::memory_order_seq_cst);
    // FLUSH(&r1);
    //  FENCE;
     std::atomic_thread_fence(std::memory_order_release);
     firstSendDone.store(false, std::memory_order_release);
    // sleep(1);
  }

  // std::thread t1(consumer, queue_offset1, std::ref(offset_1));
  // sleep(1);
  // std::thread t2(consumer_t2, queue_offset2, std::ref(offset_2));
  t1.join();

  t2.join();

  std::cout << "1111: status" << std::endl;
  // auto status = offset_2.get_future().get();
  // std::cout  << "1111: status" << status <<"r1.get_tbr()->pptr"
  // <<r1.get_tbr()->pptr << std::endl; result = (status == r1.get_tbr()->pptr);

  shmctl(shm_id, IPC_RMID, NULL);

  return print_test_summary();
}