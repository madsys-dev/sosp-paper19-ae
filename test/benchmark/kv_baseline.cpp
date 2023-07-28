#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <vector>
#include <thread>
#include <unistd.h>
#include <sys/time.h>

#include "tbb/concurrent_hash_map.h"

#include "AllocatorMacro.hpp"
#include "generator.hpp"
#include "fred.h"
#include "timer.h"

using namespace tbb;

int niterations = 1000000;
int nthreads = 8;
int read_ratio = 0;
pthread_barrier_t barrier;
HL::Timer t;


struct MyHashCompare
{
    static size_t hash(const uint64_t& key)
    {
        return (key & ((1ull << 17) - 1));
    }

    static bool equal(const uint64_t& lhs, const uint64_t& rhs)
    {
        return lhs == rhs;
    }
};

concurrent_hash_map<uint64_t, uint64_t, MyHashCompare> m[64];

extern "C" void * worker (void * arg)
{
#ifdef THREAD_PINNING
    uint64_t task_id;
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
    RandomGen r(niterations*2);
    pthread_barrier_wait(&barrier);
    if(*(int*)arg == 0) t.start();
    int i = niterations;
    int len = 1 + read_ratio;
    while(i > 0)
    {
        // std::cout<<i<<std::endl;
        uint64_t key = (task_id<<56) + r.get_num();
        uint64_t value = r.get_num();
        concurrent_hash_map<uint64_t, uint64_t, MyHashCompare>::value_type hash_pair(key, value);
        m[task_id].insert(hash_pair);
        for(size_t j=0; j<read_ratio; j++)
        {
            uint64_t id = r.get_num() & 63;
            uint64_t key = (id<<56) + r.get_num();
            concurrent_hash_map<uint64_t, uint64_t, MyHashCompare>::const_accessor hash_accessor;
            m[id].find(hash_accessor, key);
        }
        i -= len;
    }
    if(i<0)
    {
        i += len;
        while(i--)
        {
            uint64_t id =  r.get_num() & 63;
            uint64_t key = (id<<56) + r.get_num();
            concurrent_hash_map<uint64_t, uint64_t, MyHashCompare>::const_accessor hash_accessor;
            m[id].find(hash_accessor, key);
        }
    }

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

    printf( "Time elapsed = %f, throughput: %lf MOPS\n", (double) t, (double)(niterations*nthreads) / 1000000 / (double) t);

    delete [] threads;
    return 0;
}