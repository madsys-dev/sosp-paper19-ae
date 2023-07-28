#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <vector>
#include <thread>
#include <unistd.h>
#include <sys/time.h>

#include "AllocatorMacro.hpp"
#include "fred.h"

int niterations = 1000000;
int nthreads = 1;
uint64_t* queue_offset = NULL;
size_t length;
int shm_id;
int payload_size = 8;

pthread_barrier_t barrier;
struct timeval starttime,endtime;

void add(void* task, cxl_shm* shm)
{
    uint64_t* embedded_ref = (uint64_t*) task;
    int* a = (int*) get_data_at_addr(shm->get_start(), *(embedded_ref)+sizeof(CXLObj));
    int* b = a + 1;
    int* c = (int*) get_data_at_addr(shm->get_start(), *(embedded_ref+1)+sizeof(CXLObj));
    (*c) = (*a) + (*b);
}

void minus(void* task, cxl_shm* shm)
{
    uint64_t* embedded_ref = (uint64_t*) task;
    int* a = (int*) get_data_at_addr(shm->get_start(), *(embedded_ref)+sizeof(CXLObj));
    int* b = a + 1;
    int* c = (int*) get_data_at_addr(shm->get_start(), *(embedded_ref+1)+sizeof(CXLObj));
    (*c) = (*a) - (*b);
}


void mul(void* task, cxl_shm* shm)
{
    uint64_t* embedded_ref = (uint64_t*) task;
    int* a = (int*) get_data_at_addr(shm->get_start(), *(embedded_ref)+sizeof(CXLObj));
    int* b = a + 1;
    int* c = (int*) get_data_at_addr(shm->get_start(), *(embedded_ref+1)+sizeof(CXLObj));
    (*c) = (*a) * (*b);
}


void div(void* task, cxl_shm* shm)
{
    uint64_t* embedded_ref = (uint64_t*) task;
    int* a = (int*) get_data_at_addr(shm->get_start(), *(embedded_ref)+sizeof(CXLObj));
    int* b = a + 1;
    int* c = (int*) get_data_at_addr(shm->get_start(), *(embedded_ref+1)+sizeof(CXLObj));
    (*c) = (*a) / (*b);
}

void(*func_vec[])(void*, cxl_shm*) = {add, minus, mul, div};

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
	cxl_shm shm = cxl_shm(length, cxl_mem);
#else
    cxl_shm shm = cxl_shm(length, shm_id);
#endif
    shm.thread_init();
    volatile uint64_t queue = queue_offset[shm.get_thread_id() - nthreads];
    if(*(int*) arg == nthreads) gettimeofday(&starttime,NULL);
    CXLRef ref1 = shm.cxl_malloc(32, 0);
    int i = 0;
    while(i<niterations)
    {
        CXLRef task = shm.cxl_unwrap(queue);
        if(task.get_addr() != NULL)
        {
            uint64_t* data = (uint64_t*) task.get_addr();
            uint64_t func_type = *(data+2);
            func_vec[func_type](task.get_addr(), &shm);
            i++;
        }
    }
    if(task_id == nthreads) {
        gettimeofday(&endtime,NULL);
        double timeuse = 1000000*(endtime.tv_sec - starttime.tv_sec) + (endtime.tv_usec - starttime.tv_usec);
	    timeuse /= 1000000;
        fprintf(stdout, "thread 0 tput = %lf\n", niterations * nthreads / timeuse);
    }
    return NULL;

}

int main(int argc, char * argv[])
{
    length = (ZU(1) << 34);
#ifndef USE_CXL_DEV
    shm_id = shmget(10, length, IPC_CREAT);
#endif

    int shm_id_2 = shmget(100, (ZU(1) << 10), IPC_CREAT);
    queue_offset = (uint64_t*) shmat(shm_id_2, NULL, 0);


    HL::Fred * threads;
    if (argc >= 2) {
        nthreads = atoi(argv[1]);
    }

    if (argc >= 3) {
        payload_size = atoi(argv[2]);
    }

    printf("running rpc test for %d threads, %d iterations\n", nthreads, niterations);
    
    pthread_barrier_init(&barrier,NULL,nthreads);
    threads = new HL::Fred[nthreads];


    int i;
    int *threadArg = (int*)malloc(nthreads*sizeof(int));
    for (i = 0; i < nthreads; i++) {
        threadArg[i] = i + nthreads;
        threads[i].create (worker, &threadArg[i]);
    }

    for (i = 0; i < nthreads; i++) {
        threads[i].join();
    }
    gettimeofday(&endtime,NULL);
    double timeuse = 1000000*(endtime.tv_sec - starttime.tv_sec) + (endtime.tv_usec - starttime.tv_usec);
	timeuse /= 1000000;
    fprintf(stdout, "niterations = %d, time use = %lf\n", niterations, timeuse);
    fprintf(stdout, "throughput = %lf\n", 1.0 * niterations * nthreads / timeuse);
    delete [] threads;
    shmdt(queue_offset);
    return 0;
}