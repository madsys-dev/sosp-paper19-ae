#include "tatp.h"
#include "generator.hpp"
#include <sys/time.h>

#define PSIZE (1<<30)
#define VSIZE (1<<30)

#define POPULATION 1000000

size_t length;
int shm_id;
int nthreads = 2;
int niterations = 10000000;
pthread_barrier_t barrier;

struct info
{
    int task_id;
};


void tatp_init(struct TatpBenchmark* tatp, int population)
{
    tatp->population = POPULATION;
#ifdef USE_CXL_DEV
    void *cxl_mem = get_cxl_mm(length);
    tatp->shm = new cxl_shm(length, cxl_mem);
#else
    tatp->shm = new cxl_shm(length, shm_id);
#endif
    tatp->shm->thread_init();

    uint64_t id = tatp->shm->get_thread_id() - 1;
    tatp->sr = (struct Subscriber*) malloc(tatp->population*sizeof(struct Subscriber));
    for(int i=0; i<tatp->population; i++)
    {
        tatp->sr[i].s_id = i;
        tatp->sr[i].vlr_location = rand();

        uint64_t key = (id<<56) + i;
        long value = (long)&tatp->sr[i];
        tatp->shm->put(key, value);

        FENCE;
    }
}

inline
void updateLocation(struct TatpBenchmark* tatp, uint64_t s_id, long n_loc)
{
    uint64_t value;
    tatp->shm->get(s_id, value);
    struct Subscriber* sr = (struct Subscriber*) value;
    long old_loc;
    long new_loc;
    do
    {
        old_loc = sr->vlr_location.load();
        new_loc = n_loc;
    } while (!sr->vlr_location.compare_exchange_weak(old_loc, new_loc));
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
    task_id = ((info*)arg)->task_id;
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
    struct TatpBenchmark* tatp = new TatpBenchmark;
    tatp_init(tatp, POPULATION);
    unsigned short seed[3];
    seed[0] = rand();
    seed[1] = rand();
    seed[2] = rand();
    double* random = (double*) malloc(niterations*3*sizeof(double));
    for(int i=0; i<niterations*3; i++) random[i] = erand48(seed);
    pthread_barrier_wait(&barrier);
    int i = 0;
    while(i < niterations)
    {
        uint64_t id = (int)(random[3*i] * nthreads);
        uint64_t s_id = (id<<56) + (int)(random[3*i+1]*tatp->population);
        long n_loc = (int)(random[3*i+2]*0x7fffffff);
        updateLocation(tatp, s_id, n_loc);
        i++;
    }

    free(random);
    delete tatp;
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
    // int *threadArg = (int*)malloc(nthreads*sizeof(int));
    info *threadArg = (info*)malloc(nthreads*sizeof(info));
    for (i = 0; i < nthreads; i++) {
        threadArg[i].task_id = i;
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