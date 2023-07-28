#include <stdio.h>
#include "benchmark/AllocatorMacro.hpp"
#include "recovery.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <random>


typedef uintptr_t* random_t;

static uintptr_t pick(random_t r) {
  uintptr_t x = *r;
  // by Sebastiano Vigna, see: <http://xoshiro.di.unimi.it/splitmix64.c>
  x ^= x >> 30;
  x *= 0xbf58476d1ce4e5b9UL;
  x ^= x >> 27;
  x *= 0x94d049bb133111ebUL;
  x ^= x >> 31;
  *r = x;
  return x;
}

static bool chance(size_t perc, random_t r) {
  return (pick(r) % 100 <= perc);
}

static CXLRef* alloc_items(size_t items, random_t r, cxl_shm* shm) {
  if (items==0) items = 1;
  CXLRef* ref = new CXLRef(shm->cxl_malloc(items*sizeof(uintptr_t), 0));
  return ref;
}

uint64_t* queue_offset = NULL;
uint64_t nthreads = 8;
uint64_t* get_queue(int i, int j)
{
    return queue_offset + (i-1)*nthreads + (j-1);
}

int main(int argc, char * argv[])
{
    int thread_id;
    if (argc >= 2) thread_id = atoi(argv[1]);
    int shm_id_queue = shmget(100, (ZU(1) << 17), IPC_CREAT);
    if(shm_id_queue == -1)
    {
        std::cout<<"queue = NULL"<<std::endl;
        perror("shmget:");
    }
    queue_offset = (uint64_t*) shmat(shm_id_queue, NULL, 0);

#ifdef THREAD_PINNING
    int task_id;
    int core_id;
    cpu_set_t cpuset;
    int set_result;
    int get_result;
    CPU_ZERO(&cpuset);
    task_id = thread_id;
    core_id = PINNING_MAP[task_id%80];
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

    size_t length = (ZU(1) << 33);
    int shm_id = shmget(ftok(".", 1), length, IPC_CREAT);
    if(shm_id == -1)
    {
        perror("shmget:");
    }
    cxl_shm* shm = new cxl_shm(length, shm_id);
    shm->thread_init();
    intptr_t tid = shm->get_thread_id();
    uintptr_t r = std::random_device{}() % 65536 + 1; // rand();
    const size_t max_item_shift = 4; // 64
    const size_t max_item_retained_shift = max_item_shift + 2;
    uint64_t allocs = 1000 * (r % 8 + 1);
    size_t retain = allocs / 2;
    size_t data_size = 100000;
    CXLRef** data = (CXLRef**)malloc(data_size * sizeof(CXLRef*));
    size_t data_top = 0;
    CXLRef** retained = (CXLRef**)malloc(retain*sizeof(CXLRef*));
    size_t retain_top = 0;
    
    while (allocs > 0 || retain > 0)
    {
        for(size_t i=1; i<=nthreads; i++)
        {
            uint64_t* q = get_queue(i, shm->get_thread_id());
            if(*q != 0)
            {
                data[data_top] = new CXLRef(shm->cxl_unwrap(*q));
                if(data[data_top]->get_tbr() != NULL)
                {
                    data_top++;
                }
                else
                {
                    delete data[data_top];
                    data[data_top] = NULL;
                }
            }
        }

        if (retain == 0 || (chance(50, &r) && allocs > 0)) {
            allocs--;
            if (data_top >= data_size) {
                data_size += 100000;
                data = (CXLRef**)realloc(data, data_size * sizeof(CXLRef*));
            }
            size_t items = 1ULL << (pick(&r) % max_item_shift);
            if(items == 0) items = 1;
            data[data_top++] = new CXLRef(shm->cxl_malloc(items*sizeof(uintptr_t), 0));
        }
        else {
            size_t items = 1ULL << (pick(&r) % max_item_retained_shift);
            if(items == 0) items = 1;
            retained[retain_top++] = new CXLRef(shm->cxl_malloc(items*sizeof(uintptr_t), 0));
            retain--;
        }

        if (chance(66, &r) && data_top > 0) {
            size_t idx = pick(&r) % data_top;
            if(data[idx] != NULL)
            {
                delete data[idx];
                data[idx] = NULL;
            }
        }

        if (chance(30, &r) && data_top > 0)
        {
            size_t thread_to =  pick(&r) % nthreads + 1;
            while(thread_to == shm->get_thread_id()) thread_to =  pick(&r) % nthreads + 1;
            size_t data_idx = pick(&r) % data_top;
            size_t cnt = 100;
            while(data[data_idx] == NULL && cnt > 0) 
            {
                data_idx = pick(&r) % data_top;
                cnt --;
            }
            if(data[data_idx] == NULL) continue;
            uint64_t* q = get_queue(shm->get_thread_id(), thread_to);
            if(*q == 0)
            {
                *q = shm->create_msg_queue(thread_to);
            }
            shm->sent_to(*q, *data[data_idx]);
        }
    }

    for(size_t i=0; i<retain_top; i++) {
        delete retained[i];
        retained[i] = NULL;
    }
    for (size_t i = 0; i < data_top; i++) {
        if(data[i] != NULL)
        {
            delete data[i];
            data[i] = NULL;
        }
    }

    free(retained);
    free(data);
    delete shm;

    return 0;
}