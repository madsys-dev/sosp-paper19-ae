#include <iostream>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>


#include "fred.h"
#include "timer.h"
#include "AllocatorMacro.hpp"


// #define USE_CXL_DEV

int niterations = 1000;	// Default number of iterations.
int nobjects = 100000;  // Default number of objects.
int nthreads = 64;	// Default number of threads.
int work = 0;		// Default number of loop iterations.
int sz = 8;

class Foo {
public:
  Foo (void)
    : x (14),
      y (29)
    {}

  int x;
  int y;
};


size_t length;
int shm_id;
void *cxl_mem;

extern "C" void * worker (void * arg)
{
#ifdef THREAD_PINNING
    int task_id;
    int core_id;
    cpu_set_t cpuset;
    int set_result;
    int get_result;
    CPU_ZERO(&cpuset);
    task_id = *(int*)arg;
    // core_id = PINNING_MAP[task_id%80];
    core_id = get_core_id(task_id);
    CPU_SET(core_id, &cpuset);
    set_result = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (set_result != 0){
    	fprintf(stderr, "setaffinity failed for thread %d to cpu %d\n", task_id, core_id);
	exit(1);
    }
    get_result = pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (set_result != 0){
    	fprintf(stderr, "getaffinity failed for thread %d to cpu %d\n", task_id, core_id);
	exit(1);
    }
    if (!CPU_ISSET(core_id, &cpuset)){
   	fprintf(stderr, "WARNING: thread aiming for cpu %d is pinned elsewhere.\n", core_id);	 
    } else {
    	// fprintf(stderr, "thread pinning on cpu %d succeeded.\n", core_id);
    }
#endif
  int i, j;
#ifdef USE_CXL_DEV
	cxl_shm* shm = new cxl_shm(length, cxl_mem);
#else
  cxl_shm* shm = new cxl_shm(length, shm_id);
#endif
  shm->thread_init();
  CXLRef * a = (CXLRef *) malloc(sizeof(CXLRef) * (nobjects / nthreads));
  for (j = 0; j < niterations; j++) {

    // printf ("%d\n", j);
    for (i = 0; i < (nobjects / nthreads); i ++) {
      new(a+i)CXLRef(shm->cxl_malloc(sz*sizeof(Foo), 0));
      for (volatile int d = 0; d < work; d++) {
	volatile int f = 1;
	f = f + f;
	f = f * f;
	f = f + f;
	f = f * f;
      }
      assert (a+i);
    }

    for (i = 0; i < (nobjects / nthreads); i ++) {
      (a+i)->~CXLRef_s();
      for (volatile int d = 0; d < work; d++) {
	volatile int f = 1;
	f = f + f;
	f = f * f;
	f = f + f;
	f = f * f;
      }
    }
  }

  free(a);


  return NULL;
}


// void consumer_wrc(uint64_t queue_offset, std::promise<uint64_t> &offset, std::promise<uint64_t> &t_receiver)
// {
//     sleep(3);
//     cxl_shm shm = cxl_shm(length, shm_id);
//     shm.thread_init();
//     void* start = shm.get_start();
//     CXLRef r1 = shm.cxl_unwrap_wrc(queue_offset);
//     auto t_receiver_temp = static_cast<uint64_t>(time(NULL));
//     uint64_t obj_offset = r1.data;
//     CXLObj* cxl_obj1 = (CXLObj*)((uintptr_t)start + obj_offset);
//     while (cxl_obj1->writer_count != 0) {
//         cxl_obj1->reader_count++;
//     }
//     offset.set_value(r1.get_tbr()->pptr);
//     t_receiver.set_value(t_receiver_temp);
// }

int main (int argc, char * argv[])
{
  
  // using namespace std;
  // CHECK_BODY("t1 to t2") {
  //       std::cout << "t1 to t2 1" << std::endl;
  //       //todo ： 发送的时间-最后一个receiver收到的时间
  //       CXLRef r1 = shm.cxl_malloc_wrc(1, 0);
        
  //       std::cout << "t1 to t2 11" << std::endl;
  //       void* start = shm.get_start();
        
  //       std::cout << "t1 to t2 111" << std::endl;
  //       r1.str_content = "aaa";
        
  //       std::cout << "t1 to t2 1111" << std::endl;
  //       uint64_t queue_offset = shm.create_msg_queue(2);
        
  //       std::cout << "t1 to t2 11111" << std::endl;
        
        
        
  //       std::cout << "t1 to t2 2" << std::endl;
  //       uint64_t obj_offset = r1.data;
  //       CXLObj* cxl_obj = (CXLObj*)((uintptr_t)start + obj_offset);
  //       // 起t1，循环等待queue的对象
  //       std::promise<uint64_t> offset_2;
  //       std::promise<uint64_t> t_receiver;
  //       std::thread t1(consumer_wrc, queue_offset, std::ref(offset_2), std::ref(t_receiver));
        
        
  //       std::cout << "t1 to t2 3" << std::endl;
  //       while (cxl_obj->reader_count != 0 && cxl_obj->writer_count != 0) {
            
  //       }
  //       if (cxl_obj->reader_count == 0 && cxl_obj->writer_count == 0) {
  //           cxl_obj->writer_count++;
  //           r1 = shm.cxl_malloc_wrc(100, 0);
  //           RootRef* tbr1 = (RootRef*) get_data_at_addr(start, r1.tbr);
  //           tbr1->ref_cnt++;
  //           r1.str_content = "bbb";
  //           cxl_obj->writer_count--;
  //       }
            
  //       std::cout << "t1 to t2 4" << std::endl;
  //       auto t_send = static_cast<uint64_t>(time(NULL));
  //       shm.sent_to(queue_offset, r1);
  //       t1.join();
  //       auto t_receive_2 = time(NULL);
  //       auto status = offset_2.get_future().get();
  //       auto t_real_receive = t_receiver.get_future().get();
  //       auto t_all = t_real_receive - t_send;
  //       auto t_all_2 = t_receive_2 - t_send;
    

    
  //       std::cout << "t_all " << t_all << std::endl;
  //       std::cout << "t_all_2" << t_all_2 << std::endl;

  //       std::cout << "t1 to t2 5" << std::endl;
  //       CXLRef r1_t2 = shm.get_ref(status);
  //       uint64_t obj_offset_t2 = r1_t2.data;
  //       CXLObj* cxl_obj_t2 = (CXLObj*)((uintptr_t)start + obj_offset);
  //       cxl_obj_t2->reader_count--;
  //       result = (status == r1.get_tbr()->pptr);
  //       std::cout << "t1 to t2 6" << std::endl;
        
  //   }
  
//   HL::Fred * threads;
//   //pthread_t * threads;

//   if (argc >= 2) {
//     nthreads = atoi(argv[1]);
//   }

//   if (argc >= 3) {
//     niterations = atoi(argv[2]);
//   }

//   if (argc >= 4) {
//     nobjects = atoi(argv[3]);
//   }

//   if (argc >= 5) {
//     work = atoi(argv[4]);
//   }

//   if (argc >= 6) {
//     sz = atoi(argv[5]);
//   }

// #ifdef USE_CXL_DEV
// 	pm_init(length, &cxl_mem);
// #else
//   pm_init(length, shm_id);
// #endif

//   printf ("Running threadtest for %d threads, %d iterations, %d objects, %d work and %d sz...\n", nthreads, niterations, nobjects, work, sz);


//   threads = new HL::Fred[nthreads];
//   // threads = new hoardThreadType[nthreads];
//   //  hoardSetConcurrency (nthreads);

//   HL::Timer t;
//   //Timer t;

//   t.start ();

//   int i;
//   int *threadArg = (int*)malloc(nthreads*sizeof(int));
//   for (i = 0; i < nthreads; i++) {
//     threadArg[i] = i+26;
//     threads[i].create (worker, &threadArg[i]);
//   }

//   for (i = 0; i < nthreads; i++) {
//     threads[i].join();
//   }
//   t.stop ();

//   printf( "Time elapsed = %f\n", (double) t);

//   delete [] threads;
//   pm_close();
  shmctl(shm_id, IPC_RMID, NULL);
  return 0;
}

