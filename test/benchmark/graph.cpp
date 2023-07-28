#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <vector>
#include <thread>
#include <unistd.h>
#include <sys/time.h>
#include <fstream>
#include <iostream>
#include <algorithm>

#include "AllocatorMacro.hpp"
#include "generator.hpp"
#include "fred.h"
#include "timer.h"

enum QUERY_MODE {
    INTERSECT,
    TWO_HOP
};

int niterations = 1000000;
int nthreads = 8;
int query_mode = 0;
size_t length;
int shm_id;
int max_id;
pthread_barrier_t barrier;
HL::Timer t;
char *file_name;
size_t line_num;

void init_graph() {

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
#endif
#ifdef USE_CXL_DEV
    void *cxl_mem = get_cxl_mm(length);
    cxl_shm* shm = new cxl_shm(length, cxl_mem);
#else
    cxl_shm* shm = new cxl_shm(length, shm_id);
#endif
    shm->thread_init();
    srand(time(0));
    if(query_mode == INTERSECT) {
        uint64_t id = shm->get_thread_id() - 1;
        pthread_barrier_wait(&barrier);
        if(*(int*)arg == 0) t.start();
        for (int i = 0; i < niterations; i ++ ) {
            std::vector<uint64_t> vec_1, vec_2, vec_res;
            uint64_t src = rand() % max_id;
            // printf("src: %ld\n", src);
            uint64_t value;
            uint64_t count = 0;
            while (shm->get((count << 32)|src, value)) {
                // printf("dst: %ld\n", value);
                vec_1.push_back(value);
                count ++;
            }
            src = rand() % max_id;
            count = 0;
            while (shm->get((count << 32)|src, value)) {
                // printf("dst: %ld\n", value);
                vec_2.push_back(value);
                count ++;
            }
            std::sort(vec_1.begin(), vec_1.end());
            std::sort(vec_2.begin(), vec_2.end());
            std::set_intersection(vec_1.begin(), vec_1.end(), vec_2.begin(), \
                vec_2.end(), std::back_inserter(vec_res));
        }
    } else { 
        uint64_t id = shm->get_thread_id() - 1;
        pthread_barrier_wait(&barrier);
        if(*(int*)arg == 0) t.start();
        for (int i = 0; i < niterations; i ++ ) {
            uint64_t src = rand() % max_id;
            // printf("src: %ld\n", src);
            uint64_t value;
            uint64_t count = 0;
            while (shm->get((count << 32)|src, value)) {
                // printf("dst: %ld\n", value);
                count ++;
            }
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
        max_id = atoi(argv[2]);
    }

    if (argc >= 4)
    {
        line_num = atoll(argv[3]);
    }

    if (argc >= 5)
    {
        file_name = argv[4];
    }

    if (argc >= 6)
    {
        query_mode = atoi(argv[5]);
    }

    if(query_mode == INTERSECT) {
        niterations = 4000000;
    }
#ifdef USE_CXL_DEV
    void *cxl_mem;
    pm_init(length, &cxl_mem);
#else
    pm_init(length, shm_id);
#endif
#ifdef USE_CXL_DEV
    cxl_mem = get_cxl_mm(length);
    cxl_shm* shm = new cxl_shm(length, cxl_mem);
#else
    cxl_shm* shm = new cxl_shm(length, shm_id);
#endif
    shm->thread_init();
    std::ifstream myfile;
	myfile.open(file_name);
    uint64_t src, dst, pre_src=(1ULL<<31);
    uint64_t count = 0;
    for (int i = 0; i < line_num; i ++ ) {
        if (pre_src != src) {
            count = 0;
        }
        myfile >> src >> dst;
        // printf("%d %d\n", src, dst);
        shm->put((count << 32)|src, dst);
        count ++;
    }
    pthread_barrier_init(&barrier,NULL,nthreads);

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
#ifndef USE_CXL_DEVICE
    shmctl(shm_id, IPC_RMID, NULL);
#endif
    return 0;
}