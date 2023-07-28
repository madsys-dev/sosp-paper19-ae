#include <stdio.h>
#include "benchmark/AllocatorMacro.hpp"
#include "recovery.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <random>

int niterations = 10;
void *cxl_mem;

class Foo {
public:
  Foo (void)
    : x (14),
      y (29)
    {}

  int x;
  int y;
};

int main(int argc, char * argv[])
{

    if (argc >= 2)
    {
        niterations = atoi(argv[1]);
    }

#ifdef THREAD_PINNING
    int task_id;
    int core_id;
    cpu_set_t cpuset;
    int set_result;
    int get_result;
    CPU_ZERO(&cpuset);
    task_id = 1;
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
    #ifdef USE_CXL_DEV
    //todo: init cxl memory
    void *cxl_mem = get_cxl_mm(length);
    memset(cxl_mem, 0, length);
    cxl_shm *shm = new cxl_shm(length, cxl_mem);   
    #else
    int shm_id = shmget(ftok(".", 1), length, IPC_CREAT|0664);
    if(shm_id == -1)
    {
        perror("shmget:");
    }
    void* data_mem = shmat(shm_id, NULL, 0);
    memset(data_mem, 0, length);
    cxl_shm* shm = new cxl_shm(length, shm_id);
    #endif

    shm->thread_init();
    CXLRef * data = (CXLRef *) malloc(sizeof(CXLRef) * niterations);
    for(int i=0; i<niterations; i++)
    {
        new(data+i)CXLRef(shm->cxl_malloc(sizeof(Foo), 0));
    }
    std::cout<<"alloc done"<<std::endl;
    free(data);
    return 0;
}