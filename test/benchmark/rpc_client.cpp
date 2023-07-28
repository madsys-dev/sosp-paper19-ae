#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <vector>
#include <thread>
#include <unistd.h>

#include "AllocatorMacro.hpp"
#include "fred.h"

int niterations = 1000000;
int nthreads = 1;
size_t length;
int shm_id;
int payload_size = 8;

int input_cnt[4] = {2, 2, 2, 2};
int output_cnt[4] = {1, 1, 1, 1};
uint64_t* queue_offset = NULL;

pthread_barrier_t barrier;

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
    pthread_barrier_wait(&barrier);
#endif
#ifdef USE_CXL_DEV
    void *cxl_mem = get_cxl_mm(length);
    cxl_shm* shm = new cxl_shm(length, cxl_mem);
#else
    cxl_shm* shm = new cxl_shm(length, shm_id);
#endif
    shm->thread_init();
    uintptr_t r = ((shm->get_thread_id() + 1) * 43);
    CXLRef * input_params = (CXLRef *) malloc(sizeof(CXLRef) * niterations);
    CXLRef * output = (CXLRef *) malloc(sizeof(CXLRef) * niterations);
    CXLRef * a = (CXLRef *) malloc(sizeof(CXLRef) * niterations);
    for(size_t i=0; i<niterations; i++)
    {
        new(input_params+i)CXLRef(shm->cxl_malloc(8, 0));
        int* input_params_1 = (int *) (input_params+i)->get_addr();
        *input_params_1 = pick(&r)%1000 + 1;
        int* input_params_2 = input_params_1+1;
        *input_params_2 = pick(&r)%1000 + 1;
        new(output+i)CXLRef(shm->cxl_malloc(8, 0));
        new(a+i)CXLRef(shm->cxl_malloc(24, 2));
    }

    uint64_t queue = shm->create_msg_queue(shm->get_thread_id() + nthreads);
    queue_offset[shm->get_thread_id()] = queue;
    std::cout<<"queue offset = "<<queue<<std::endl;

    for(size_t i=0; i<niterations; i++)
    {
        int func_id = i & 3;
        uint64_t *data = (uint64_t*) (a+i)->get_addr();
        *(data+2) = func_id;
        shm->link_reference(*(data), (input_params+i)->get_tbr()->pptr);
        shm->link_reference(*(data+1), (output+i)->get_tbr()->pptr);
        while(!shm->sent_to(queue, *(a+i))) {}
    }
    for(size_t i=0; i<niterations; i++) 
        (a+i)->~CXLRef_s();
    sleep(3);
    for(size_t i=0; i<niterations; i++)
    {
        (input_params+i)->~CXLRef_s();
        (output+i)->~CXLRef_s();
    }
    shm->cxl_free(true, (cxl_block *)get_data_at_addr(shm->get_start(), queue));
    free(input_params);
    free(output);
    free(a);
    delete shm;
    return NULL;
}


int main(int argc, char * argv[])
{
    int shm_id_2 = shmget(100, (ZU(1) << 10), IPC_CREAT|0664);
    queue_offset = (uint64_t*) shmat(shm_id_2, NULL, 0);
    memset(queue_offset, 0, (ZU(1) << 10));

    srand( (unsigned)time( NULL ) );

    HL::Fred * threads;
    if (argc >= 2) {
        nthreads = atoi(argv[1]);
    }

    if (argc >= 3) {
        payload_size = atoi(argv[2]);
    }
#ifdef USE_CXL_DEV
    void *cxl_mem;
    pm_init(length, &cxl_mem);
#else
    pm_init(length, shm_id);
#endif
    printf("running rpc test for %d threads, %d iterations\n", nthreads, niterations);

    threads = new HL::Fred[nthreads];
    pthread_barrier_init(&barrier,NULL,nthreads);

    int i;
    int *threadArg = (int*)malloc(nthreads*sizeof(int));
    for (i = 0; i < nthreads; i++) {
        threadArg[i] = i;
        threads[i].create (worker, &threadArg[i]);
    }

    for (i = 0; i < nthreads; i++) {
        threads[i].join();
    }

    delete [] threads;
    pm_close();
    shmdt(queue_offset);
    shmctl(shm_id_2, IPC_RMID, NULL);
    shmctl(shm_id, IPC_RMID, NULL);
    return 0;
}