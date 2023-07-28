#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <vector>
#include <thread>
#include <unistd.h>
#include <sys/time.h>

#include "AllocatorMacro.hpp"
#include "zipf_gen.hpp"
#include "fred.h"
#include "timer.h"

int niterations = 10000000;
int nthreads = 8;
double zipf_ratio = 0.99;
size_t length;
int shm_id;
int read_ratio = 0;
pthread_barrier_t barrier;
HL::Timer t;

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
    ZipfGen r(zipf_ratio, niterations, niterations*2);
    pthread_barrier_wait(&barrier);
    if(*(int*)arg == 0) t.start();
#ifdef USE_CXL_DEV
    void *cxl_mem = get_cxl_mm(length);
    cxl_shm* shm = new cxl_shm(length, cxl_mem);
#else
    cxl_shm* shm = new cxl_shm(length, shm_id);
#endif
    shm->thread_init();
    int i = niterations;
    uint64_t id = shm->get_thread_id() - 1;
    int len = 1 + read_ratio;
    while(i > 0)
    {
        uint64_t key = (id<<56) + r.next_long();
        uint64_t value = r.next_long();
        shm->put(key, value);
        for(size_t j=0; j<read_ratio; j++)
        {
            uint64_t id = r.next_long() & 63;
            uint64_t key = (id<<56) + r.next_long();
            uint64_t value;
            shm->get(key, value);
        }
        i -= len;
    }
    if(i<0)
    {
        i += len;
        while(i--)
        {
            uint64_t id =  r.next_long() & 63;
            uint64_t key = (id<<56) + r.next_long();
            uint64_t value;
            shm->get(key, value);
        }
    }


    delete shm;
    return NULL;
}

int main(int argc, char* argv[])
{
    HL::Fred * threads;

    if (argc >= 2)
    {
        nthreads = atoi(argv[1]);
    }

    if (argc >= 3)
    {
        read_ratio = atoi(argv[2]);
    }

    if (argc >= 4) {
        zipf_ratio = atof(argv[3]);
    }

    pthread_barrier_init(&barrier,NULL,nthreads);
#ifdef USE_CXL_DEV
    void *cxl_mem;
    pm_init(length, &cxl_mem);
#else
    pm_init(length, shm_id);
#endif
    threads = new HL::Fred[nthreads];

    int i;
    int *threadArg = (int*)malloc(nthreads*sizeof(int));
    for (i = 0; i < nthreads; i++) {
        threadArg[i] = i;
        threads[i].create (worker, &threadArg[i]);
    }

    for (i = 0; i < nthreads; i++) {
        threads[i].join();
    }
    t.stop();

    printf( "Time elapsed = %f\n", (double) t);
    printf( "Throughput = %f\n", (double)niterations * nthreads / t);

    delete [] threads;

    pm_close();
    shmctl(shm_id, IPC_RMID, NULL);
    return 0;
}