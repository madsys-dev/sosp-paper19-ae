#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <vector>
#include <thread>
#include <unistd.h>
#include <sys/time.h>

#include "AllocatorMacro.hpp"
#include "generator.hpp"
#include "fred.h"
#include "timer.h"

int niterations = 10000000;
int acount_num = 1000000;
int nthreads = 8;
size_t length;
int shm_id;

pthread_barrier_t barrier;

extern "C" void * worker (void * arg)
{
#ifdef THREAD_PINNING
    int task_id;
    int core_id;
    cpu_set_t cpuset;
    int set_result;
    int get_result;
    CPU_ZERO(&cpuset);
    task_id = *((int*)arg);
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
#ifdef USE_CXL_DEV
    void *cxl_mem = get_cxl_mm(length);
    cxl_shm* shm = new cxl_shm(length, cxl_mem);
#else
    cxl_shm* shm = new cxl_shm(length, shm_id);
#endif
    shm->thread_init();
	uint64_t id = shm->get_thread_id() - 1;
	unsigned short seed[3];
    seed[0] = rand();
    seed[1] = rand();
    seed[2] = rand();
	double* random = (double*) malloc(niterations*sizeof(double)*3);
	for(int i=0; i<niterations*3; i++) random[i] = erand48(seed);
	int tail = 0;
	for(int i=0; i < acount_num; i++)
	{
		uint64_t key = (id<<56) + i;
		shm->put(key, 10000LL);
	}
	FENCE;
	pthread_barrier_wait(&barrier);
	for(int i=0; i < niterations; i++)
	{
		int src = random[3*i] * acount_num;
		int dst = random[3*i+1] * acount_num;
		uint64_t money_src, money_dst;
		shm->get((id<<56) + src, money_src);
		shm->get((id<<56) + dst, money_src);
		int money_transfer = random[3*i+2] * 100;
		shm->put((id<<56) + src, money_src + money_transfer);
		shm->put((id<<56) + dst, money_dst - money_transfer);
		FENCE;
	}
	return NULL;
}

int main(int argc, char** argv)
{
	if (argc >= 2) {
    	nthreads = atoi(argv[1]);
  	}
	HL::Fred * threads;
    pthread_barrier_init(&barrier,NULL,nthreads+1);
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
	pthread_barrier_wait(&barrier);
    struct timeval starttime,endtime;
    gettimeofday(&starttime,NULL);
	for(int i=0; i<nthreads; i++){
		threads[i].join();
	}
	gettimeofday(&endtime,NULL);
    double timeuse = 1000000*(endtime.tv_sec - starttime.tv_sec) + (endtime.tv_usec - starttime.tv_usec);
	timeuse /= 1000000;
    fprintf(stdout, "time use = %lf, throughput: %lf MOPS\n", timeuse, (double)((niterations / 1000000)*nthreads) / timeuse);
    delete [] threads;
    pm_close();
    shmctl(shm_id, IPC_RMID, NULL);
	return 0;
}